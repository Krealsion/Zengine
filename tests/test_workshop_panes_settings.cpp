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
        stage_desk(r, [&](Setup& desk) {
            REQUIRE(add_pane(desk, dial_ref()));
        });
        stage_desk(r, [&](Setup& desk) {
            pane_of(desk, dial_ref())->settings = settings;
        });
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
    // ...the settings Workshop's, the room the window's that shows the desk.
    for (std::size_t i = 0; i < t.seat->authors.size(); ++i) {
        CHECK(t.seat->authors[i] == (t.seat->heard[i] == "room" ? t.r.windows.office()
                                                                : std::string(kWorkshopProvider)));
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
    stage_desk(r, [&](Setup& desk) {
        REQUIRE(add_pane(desk, dial_ref()));
    });
    stage_desk(r, [&](Setup& desk) {
        pane_of(desk, dial_ref())->settings = {legend_off()};
    });
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
    stage_desk(t.r, [&](Setup& desk) {
        pane_of(desk, dial_ref())->settings.clear();
    });
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
    REQUIRE(t.r.owner->layouts().counted_v1(dial_ref()) != nullptr);
    CHECK(*t.r.owner->layouts().counted_v1(dial_ref()) == dial_declared().rows);

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
        CHECK(*t.r.owner->layouts().counted_v1(dial_ref()) == dial_declared().rows);
    }

    // A LATER ACCEPTED DECLARATION REPLACES THE SETTINGS WHOLE.
    t.declare(with({flag_row("legend", false)}));
    CHECK(*t.r.owner->layouts().counted_v1(dial_ref()) ==
          std::vector<PaneSettingRow>{flag_row("legend", false)});

    // ...AND PERSONAL SPEECH, EVEN FROM THE HOLDER, DECLARES NOTHING AND SAYS NOTHING.
    const std::string before = t.r.last_notice();
    t.r.drive(t.seat, [](SettingsSeat& s, loom::Mail& m) { s.declare_personally(m, dial_declared()); });
    CHECK(*t.r.owner->layouts().counted_v1(dial_ref()) ==
          std::vector<PaneSettingRow>{flag_row("legend", false)});
    CHECK(t.r.last_notice() == before);
}

TEST_CASE("a declaration counts only while the weave that sent it holds the office") {
    DialRig t;
    t.open({});
    t.offer();
    t.declare(dial_declared());
    REQUIRE(t.r.owner->layouts().counted_v1(dial_ref()) != nullptr);

    // ANOTHER WEAVE TAKES THE OFFICE: the declaration stays where it was admitted and counts for
    // nothing, until the new holder declares its own.
    t.r.unmount_settings_seat(t.seat);
    SettingsSeat* successor = t.r.mount_settings_seat(kDialOffice);
    CHECK(t.r.owner->layouts().counted_v1(dial_ref()) == nullptr);
    PaneSettingsDeclared own;
    own.pane = kDial;
    own.rows = {flag_row("legend", false)};
    t.r.drive(successor, [own](SettingsSeat& s, loom::Mail& m) { s.declare(m, own); });
    REQUIRE(t.r.owner->layouts().counted_v1(dial_ref()) != nullptr);
    CHECK(*t.r.owner->layouts().counted_v1(dial_ref()) == own.rows);
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
    CHECK(info_labels(t.r, dial_ref()) == std::vector<std::string>{"legend", "mode", "step", "zoom"});
    std::vector<std::string> sections;
    for (const Row& row : t.r.session().inspected.rows) {
        if (row.section()) {
            sections.push_back(row.label());
        }
    }
    CHECK(sections == std::vector<std::string>{"AUTHORED", "RESOLVED", "INTERIOR"});
    // NOTHING STORED READS AS THE DEFAULT, SPELLED AS IT IS TYPED.
    CHECK(info_value(t.r, dial_ref(), "legend") == "on");
    CHECK(info_value(t.r, dial_ref(), "mode") == "wide");
    CHECK(info_value(t.r, dial_ref(), "step") == "1");
    CHECK(info_value(t.r, dial_ref(), "zoom") ==
          "on -- kept; no setting here takes it, and `-` clears it");
    for (const char* label : {"legend", "mode", "step", "zoom"}) {
        CAPTURE(label);
        CHECK(info_has(t.r, dial_ref(), label));
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
        const PaneSubjectActed said = info_commit(t.r, dial_ref(), c.label, c.typed);
        CHECK_FALSE(said.accepted);
        CHECK(said.refusal == c.says);
        CHECK(t.stored().empty());
    }
}

TEST_CASE("a setting typed at its default is stored as its absence, and `-` clears one") {
    InspectedDial t;
    t.open_inspected();
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "off").accepted);
    REQUIRE(info_commit(t.r, dial_ref(), "step", "3").accepted);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"legend", false, {}, {}},
                                                 PaneSetting{"step", {}, 3, {}}});
    CHECK(info_value(t.r, dial_ref(), "legend") == "off");
    // THE DEFAULT, TYPED, IS NOTHING STORED: one spelling of "the pane's default".
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "on").accepted);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 3, {}}});
    REQUIRE(info_commit(t.r, dial_ref(), "step", " - ").accepted);
    CHECK(t.stored().empty());
    // ...AND `-` WITH NOTHING STORED SAYS SO, writing nothing.
    const PaneSubjectActed again = info_commit(t.r, dial_ref(), "step", "-");
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
    REQUIRE(info_commit(t.r, dial_ref(), "side", "left ").accepted);
    CHECK(stored() == text("side", "left "));
    CHECK(info_value(t.r, dial_ref(), "side") == "left ");
    CHECK(t.seat->handed.back().settings == text("side", "left "));
    // ...AND A DRAFT OPENED ON IT AND COMMITTED UNCHANGED LEAVES IT AS IT IS.
    REQUIRE(info_commit(t.r, dial_ref(), "side", info_value(t.r, dial_ref(), "side"))
                .accepted);
    CHECK(stored() == text("side", "left "));

    // THREE SPACES ARE A TEXT OF THREE SPACES, not an empty one.
    REQUIRE(info_commit(t.r, dial_ref(), "side", "   ").accepted);
    CHECK(stored() == text("side", "   "));

    // A CHOICE IS TAKEN EXACTLY: one with a space the setting does not name is refused, saying
    // what it takes, and nothing is written.
    const PaneSubjectActed spaced = info_commit(t.r, dial_ref(), "mode", "narrow ");
    CHECK_FALSE(spaced.accepted);
    CHECK(spaced.refusal == "mode: takes one of wide, narrow, not `narrow `");
    CHECK(stored() == text("side", "   "));

    // `-`, WITH SPACES AROUND IT OR NOT, CLEARS -- as it resets every row Info edits.
    REQUIRE(info_commit(t.r, dial_ref(), "side", " - ").accepted);
    CHECK(stored().empty());
}

TEST_CASE("a settings write says it was stored and handed, or why it was not handed") {
    InspectedDial t;
    t.open_inspected();
    const std::size_t heard = t.seat->heard.size();
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "off").accepted);
    CHECK(t.r.last_notice() == "committed legend of Dial = off; stored and handed");
    // HANDED IN THE COMMIT'S OWN DELIVERY, before any room it grants.
    REQUIRE(t.seat->heard.size() == heard + 1);
    CHECK(t.seat->heard.back() == "settings");
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{PaneSetting{"legend", false, {}, {}}});

    // NOTHING HOLDING THE OFFICE: the declared row is a kept one now, and `-` clears it; stored,
    // and the sentence says nobody holds the office.
    t.r.unmount_settings_seat(t.seat);
    t.r.key(input::scan::kUnknown);
    const PaneSubjectActed cleared = info_commit(t.r, dial_ref(), "legend", "-");
    REQUIRE_MESSAGE(cleared.accepted, cleared.refusal);
    CHECK(t.r.last_notice() ==
          "cleared legend of Dial; stored; not handed: nothing holds `test.dial` now");

    // A HOLDER WITHOUT THE DOOR: stored, and the sentence says why it went no further.
    stage_desk(t.r, [](Setup& desk) {
        pane_of(desk, dial_ref())->settings = {PaneSetting{"legend", false, {}, {}}};
    });
    ProviderSeat* plain = t.r.mount_provider(kDialOffice);
    t.r.drive(plain, [](ProviderSeat& p, loom::Mail& m) { p.offer(m, dial_offer()); });
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "-").accepted);
    CHECK(t.r.last_notice() ==
          "cleared legend of Dial; stored; not handed: test.dial takes no settings");
}

TEST_CASE("a kept setting takes only `-`, and the rows are named anew when it goes") {
    InspectedDial t;
    t.open_inspected({PaneSetting{"zoom", true, {}, {}}});
    const std::int64_t revision = info_read(t.r, dial_ref()).revision;
    const PaneSubjectActed no = info_commit(t.r, dial_ref(), "zoom", "off");
    CHECK_FALSE(no.accepted);
    CHECK(no.refusal == "zoom: kept for a pane that declares it -- it takes only `-`, which clears it");
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"zoom", true, {}, {}}});
    REQUIRE(info_commit(t.r, dial_ref(), "zoom", "-").accepted);
    CHECK(t.stored().empty());
    // THE ROW WENT WITH ITS KEY, so the rows are read anew, at the revision it moved to.
    CHECK(info_labels(t.r, dial_ref()) == std::vector<std::string>{"legend", "mode", "step"});
    CHECK(info_read(t.r, dial_ref()).revision != revision);
}

TEST_CASE("the row Info reads moves with what it is built from: a declaration, and a new holder") {
    // ⚔ MUTATION: the revision minted without the counted declaration -- a declaration arriving
    // leaves the reading's revision, and a commit judged before it is written.
    DialRig t;
    t.open({});
    t.offer();
    // NOTHING DECLARED, NOTHING STORED: no SETTINGS rows at all.
    CHECK(info_labels(t.r, dial_ref()).empty());
    const std::int64_t bare = info_read(t.r, dial_ref()).revision;

    t.declare(dial_declared());
    const std::int64_t declared = info_read(t.r, dial_ref()).revision;
    CHECK(declared != bare);
    REQUIRE(info_has(t.r, dial_ref(), "legend"));

    // AN IDENTICAL DECLARATION, as every catalog request brings, keeps the revision...
    t.declare(dial_declared());
    CHECK(info_read(t.r, dial_ref()).revision == declared);

    // ...AND A COMMIT JUDGED AT THE OLD ONE IS REFUSED WITH NOTHING WRITTEN.
    PaneSettingCommitRequested stale;
    stale.provider = kDialOffice;
    stale.pane = kDial;
    stale.revision = bare;
    stale.key = "legend";
    stale.flag = false;
    t.r.owner->read_host_now();
    CHECK_FALSE(t.r.owner->store().commit_pane(std::string(), stale).accepted);
    CHECK(pane_of(live_desk(t.r.owner->layouts().run()), dial_ref())->settings.empty());

    // A NEW HOLDER: the declaration stops counting, and its rows become kept ones.
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "off").accepted);
    t.r.unmount_settings_seat(t.seat);
    (void)t.r.mount_settings_seat(kDialOffice);
    t.r.key(input::scan::kUnknown);
    CHECK(info_read(t.r, dial_ref()).revision != declared);
    CHECK(info_value(t.r, dial_ref(), "legend") ==
          "off -- kept; no setting here takes it, and `-` clears it");
}

TEST_CASE("the subject is named anew when its pane's row leaves the desk, and again when it comes back") {
    InspectedDial t;
    t.open_inspected();
    const std::int64_t before = t.r.session().inspected.name;
    const std::size_t at = subject_row_index(t.r.session(), "X", "AUTHORED");
    REQUIRE(at < t.r.session().inspected.rows.size());
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
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "off").accepted);
    REQUIRE(hand_close(t.r, dial_ref()).closed);
    CHECK(t.r.last_notice().find("hid Dial and its settings here (legend off) -- ") == 0);
    REQUIRE(seat_pane(t.r, dial_ref()).opened);
    CHECK(t.stored().empty());

    REQUIRE(info_commit(t.r, dial_ref(), "step", "4").accepted);
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
    stage_desk(t, [](Setup& desk) {
        pane_of(desk, ref_of(stock::kKind))->settings = {
            PaneSetting{"mode", {}, {}, std::string("wide")}};
    });
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
    stage_run(t.r, [](SetupState& run) { link_live_setup(run, "/kept/desk.json"); });
    t.r.key(input::scan::kEscape); // a delivery, so the store's run is presented
    REQUIRE(live_status(t.r.session().setup) == setup_link::kCurrent);
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "off").accepted);
    CHECK(live_status(t.r.session().setup) == setup_link::kModified);
    // ...AND THE DEFAULT WRITES ITS ABSENCE, which is what the file holds: current again.
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "-").accepted);
    CHECK(live_status(t.r.session().setup) == setup_link::kCurrent);

    // A FILE THAT SPELLED THE DEFAULT IS NOT THE ABSENCE: the layout stays modified.
    stage_run(t.r, [](SetupState& run) {
        pane_of(run.active, dial_ref())->settings = {PaneSetting{"legend", true, {}, {}}};
        link_live_setup(run, "/kept/desk.json");
    });
    t.r.key(input::scan::kEscape);
    REQUIRE(info_commit(t.r, dial_ref(), "legend", "-").accepted);
    CHECK(live_status(t.r.session().setup) == setup_link::kModified);
}

// ---- v2: how a weaver finds a setting, the revision a row is handed at, and the doors ----

namespace {

/// A NATIVE SEAT THAT TAKES v2's SETTINGS: it holds an office, offers and declares as it is told,
/// and records in order every room and every hand-off with its revision; as an office it also asks
/// the settings doors and keeps their answers. Apart from `SettingsSeat`, which takes v1.
class RevisionSeat
    : public loom::WeaveBase<RevisionSeat, SeatState,
                             loom::Accept<PaneCatalogRequested, gfx::AreaRoom, v2::PaneSettings,
                                          PaneSettingsAnswered, PaneSettingCommitAnswered, OfficeSettings,
                                          OfficeSettingsAnswered, OfficeSettingCommitAnswered,
                                          PaneSettingPinAnswered,
                                          PaneSettingLockAnswered, LayoutSettingsAnswered,
                                          SettingsWriteAnswered, gfx::AreaButtonChanged, SeatDo>,
                             loom::Emit<PaneOffered, v2::PaneSettingsDeclared,
                                        PaneSettingsRequested, PaneSettingCommitRequested,
                                        PaneSettingUnused, OfficeSettingsDeclared,
                                        OfficeSettingsRequested, OfficeSettingCommitRequested,
                                        PaneSettingPinRequested, PaneSettingLockRequested,
                                        LayoutSettingsRequested, SettingsWriteRequested>> {
public:
    explicit RevisionSeat(std::string office) : office_(std::move(office)) {}

    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const gfx::AreaRoom&, loom::Mail&) { heard.push_back("room"); }
    void on(const v2::PaneSettings& s, loom::Mail& mail) {
        heard.push_back("settings");
        handed.push_back(s);
        authors.push_back(std::string(mail.authored_role()));
    }
    void on(const PaneSettingsAnswered& s, loom::Mail& mail) {
        if (mail.answers_ask()) shown.push_back(s);
    }
    void on(const PaneSettingCommitAnswered& a, loom::Mail& mail) {
        if (mail.answers_ask()) acted.push_back(a);
    }
    void on(const OfficeSettings& s, loom::Mail& mail) {
        office_handed.push_back(s);
        authors.push_back(std::string(mail.authored_role()));
    }
    void on(const OfficeSettingsAnswered& s, loom::Mail& mail) {
        if (mail.answers_ask()) office_shown.push_back(s);
    }
    void on(const OfficeSettingCommitAnswered& a, loom::Mail& mail) {
        if (mail.answers_ask()) office_acted.push_back(a);
    }
    void on(const PaneSettingPinAnswered& a, loom::Mail& mail) {
        if (mail.answers_ask()) pin_acted.push_back(a);
    }
    void on(const PaneSettingLockAnswered& a, loom::Mail& mail) {
        if (mail.answers_ask()) lock_acted.push_back(a);
    }
    void on(const LayoutSettingsAnswered& s, loom::Mail& mail) {
        if (mail.answers_ask()) layouts_shown.push_back(s);
    }
    void on(const SettingsWriteAnswered& w, loom::Mail& mail) {
        if (mail.answers_ask()) batches.push_back(w);
    }
    /// A PRESS THE WINDOW GAVE ITS AREA, and the number it came under: what an unlock names as
    /// the weaver's act.
    void on(const gfx::AreaButtonChanged& b, loom::Mail& mail) {
        if (b.pressed && b.button == 1) press_asks.push_back(mail.correlation());
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
            (void)mail.send_to_role(door_of<Shape>(), s);
        } else {
            (void)mail.as_role(office_).send_to_role(door_of<Shape>(), s);
        }
    }

    std::function<void(RevisionSeat&, loom::Mail&)> next;
    std::vector<std::string> heard;
    std::vector<v2::PaneSettings> handed;
    std::vector<std::string> authors;
    std::vector<PaneSettingsAnswered> shown;
    std::vector<PaneSettingCommitAnswered> acted;
    std::vector<OfficeSettingCommitAnswered> office_acted;
    std::vector<OfficeSettings> office_handed;
    std::vector<OfficeSettingsAnswered> office_shown;
    std::vector<PaneSettingPinAnswered> pin_acted;
    std::vector<PaneSettingLockAnswered> lock_acted;
    std::vector<LayoutSettingsAnswered> layouts_shown;
    std::vector<SettingsWriteAnswered> batches;
    std::vector<std::uint64_t> press_asks;

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
    grant.allow_to_any(OfficeSettingsDeclared::zen_name, OfficeSettingsDeclared::zen_version);
    grant.allow_to_any(OfficeSettingsRequested::zen_name, OfficeSettingsRequested::zen_version);
    grant.allow_to_any(OfficeSettingCommitRequested::zen_name,
                       OfficeSettingCommitRequested::zen_version);
    grant.allow_to_any(PaneSettingPinRequested::zen_name, PaneSettingPinRequested::zen_version);
    grant.allow_to_any(PaneSettingLockRequested::zen_name, PaneSettingLockRequested::zen_version);
    grant.allow_to_any(LayoutSettingsRequested::zen_name, LayoutSettingsRequested::zen_version);
    grant.allow_to_any(SettingsWriteRequested::zen_name, SettingsWriteRequested::zen_version);
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
        stage_desk(r, [&](Setup& desk) {
            REQUIRE(add_pane(desk, dial_ref()));
        });
        stage_desk(r, [&](Setup& desk) {
            pane_of(desk, dial_ref())->settings = settings;
        });
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
    PaneSettingsAnswered read(bool personally = false) {
        const std::size_t before = editor->shown.size();
        drive(r, editor_id, editor, [personally](RevisionSeat& s, loom::Mail& m) {
            s.say(m, PaneSettingsRequested{kDialOffice, kDial}, personally);
        });
        REQUIRE(editor->shown.size() == before + 1);
        return editor->shown.back();
    }
    PaneSettingCommitAnswered commit(PaneSettingCommitRequested asked, bool personally = false) {
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

    /// ONE OFFICE'S SETTINGS READ, AND ONE OF THEM WRITTEN, by the editor.
    OfficeSettingsAnswered read_office(const std::string& office) {
        const std::size_t before = editor->office_shown.size();
        drive(r, editor_id, editor, [office](RevisionSeat& s, loom::Mail& m) {
            s.say(m, OfficeSettingsRequested{office});
        });
        REQUIRE(editor->office_shown.size() == before + 1);
        return editor->office_shown.back();
    }
    OfficeSettingCommitAnswered commit_office(OfficeSettingCommitRequested asked) {
        const std::size_t before = editor->office_acted.size();
        drive(r, editor_id, editor, [asked](RevisionSeat& s, loom::Mail& m) { s.say(m, asked); });
        REQUIRE(editor->office_acted.size() == before + 1);
        return editor->office_acted.back();
    }
    /// ...AND ONE PIN ACT ON THE DIAL.
    PaneSettingPinAnswered pin(const std::string& key, const char* act, std::int64_t layout = 0) {
        const std::size_t before = editor->pin_acted.size();
        drive(r, editor_id, editor, [key, act, layout](RevisionSeat& s, loom::Mail& m) {
            s.say(m, PaneSettingPinRequested{kDialOffice, kDial, key, act, layout});
        });
        REQUIRE(editor->pin_acted.size() == before + 1);
        return editor->pin_acted.back();
    }
    /// ...AND ONE LOCK ACT, asked by the editor or -- `by_dial` -- by the dial's own office.
    PaneSettingLockAnswered lock(PaneSettingLockRequested asked, bool by_dial = false) {
        RevisionSeat* who = by_dial ? seat : editor;
        const loom::WeaveId id = by_dial ? seat_id : editor_id;
        const std::size_t before = who->lock_acted.size();
        drive(r, id, who, [asked](RevisionSeat& s, loom::Mail& m) { s.say(m, asked); });
        REQUIRE(who->lock_acted.size() == before + 1);
        return who->lock_acted.back();
    }
    /// ...AND EVERY LAYOUT READ, AND A BATCH WRITTEN, by the editor (or as no office).
    LayoutSettingsAnswered layouts(bool personally = false) {
        const std::size_t before = editor->layouts_shown.size();
        drive(r, editor_id, editor, [personally](RevisionSeat& s, loom::Mail& m) {
            s.say(m, LayoutSettingsRequested{}, personally);
        });
        REQUIRE(editor->layouts_shown.size() == before + 1);
        return editor->layouts_shown.back();
    }
    SettingsWriteAnswered batch(SettingsWriteRequested asked, bool personally = false) {
        const std::size_t before = editor->batches.size();
        drive(r, editor_id, editor, [asked, personally](RevisionSeat& s, loom::Mail& m) {
            s.say(m, asked, personally);
        });
        REQUIRE(editor->batches.size() == before + 1);
        return editor->batches.back();
    }
};

/// A lock act on the dial's `key`.
PaneSettingLockRequested dial_lock(const char* key, const char* act, const char* scope,
                                   std::int64_t layout = 0, std::int64_t gesture = 0) {
    return PaneSettingLockRequested{"test.dial/dial", key, act, scope, layout, gesture};
}

/// One write of a batch.
SettingWrite flag_write(std::int64_t layout, const char* target, const char* key, bool value) {
    SettingWrite w;
    w.layout = layout;
    w.target = target;
    w.key = key;
    w.flag = value;
    return w;
}
SettingWrite number_write(std::int64_t layout, const char* target, const char* key,
                          std::int64_t value) {
    SettingWrite w;
    w.layout = layout;
    w.target = target;
    w.key = key;
    w.number = value;
    return w;
}
SettingWrite text_write(std::int64_t layout, const char* target, const char* key,
                        const char* value) {
    SettingWrite w;
    w.layout = layout;
    w.target = target;
    w.key = key;
    w.text = std::string(value);
    return w;
}

/// The dial's row in the layout at `at`, read off the desk.
std::vector<PaneSetting> dial_settings_at(PaneRig& r, std::size_t at) {
    const SetupPane* row = pane_of(layout_at(r.run(), at), dial_ref());
    REQUIRE(row != nullptr);
    return row->settings;
}

/// A commit of Workshop's own text setting `key`, at the revision read now.
OfficeSettingCommitRequested workshop_text(std::int64_t revision, const char* key, const char* text) {
    OfficeSettingCommitRequested c;
    c.office = kWorkshopProvider;
    c.revision = revision;
    c.key = key;
    c.text = std::string(text);
    return c;
}

/// ...and of its own number setting `key`.
OfficeSettingCommitRequested workshop_number(std::int64_t revision, const char* key, std::int64_t n) {
    OfficeSettingCommitRequested c;
    c.office = kWorkshopProvider;
    c.revision = revision;
    c.key = key;
    c.number = n;
    return c;
}

PaneSettingCommitRequested number_at(std::int64_t revision, const char* key, std::int64_t n) {
    PaneSettingCommitRequested c;
    c.revision = revision;
    c.key = key;
    c.number = n;
    return c;
}

PaneSettingCommitRequested text_at(std::int64_t revision, const char* key, const char* text) {
    PaneSettingCommitRequested c;
    c.revision = revision;
    c.key = key;
    c.text = std::string(text);
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
    REQUIRE(t.r.owner->layouts().counted(dial_ref()) != nullptr);
    CHECK(*t.r.owner->layouts().counted(dial_ref()) == sound.rows);
    CHECK(*t.r.owner->layouts().counted_v1(dial_ref()) == dial_declared().rows);
    v2::PaneSettingsDeclared reserved = sound;
    reserved.rows[1].tags = {"kept"};
    t.declare(reserved);
    CHECK(t.r.last_notice() ==
          "Dial @test.dial: its settings were not taken -- setting `mode`: `kept` is a tab of its "
          "own, never a setting's tag");
    CHECK(*t.r.owner->layouts().counted(dial_ref()) == sound.rows);
}

TEST_CASE("a v1 declaration is read as v2 rows, untagged, unpinned and unsummarised") {
    DialRig t;
    t.open({});
    t.offer();
    t.declare(dial_declared());
    REQUIRE(t.r.owner->layouts().counted(dial_ref()) != nullptr);
    const std::vector<v2::PaneSettingRow>& rows = *t.r.owner->layouts().counted(dial_ref());
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
    REQUIRE(info_labels(t.r, dial_ref()) == std::vector<std::string>{"legend", "mode", "step"});
    // THE SAME IMAGE OFFERING AGAIN, as every catalog request brings, keeps its declaration.
    t.offer();
    CHECK(t.r.owner->layouts().counted_v1(dial_ref()) != nullptr);
    CHECK(info_labels(t.r, dial_ref()) == std::vector<std::string>{"legend", "mode", "step"});
    // A RELOAD IN PLACE: the same weave id, its next incarnation, offering and declaring nothing.
    const loom::WeaveId same = t.r.settings_seat_id(t.seat);
    REQUIRE(t.r.bus.swap_state(same, t.r.bus.snapshot_bytes(same)).revived);
    t.offer();
    REQUIRE(t.r.settings_seat_id(t.seat) == same);
    CHECK(t.r.owner->layouts().counted_v1(dial_ref()) == nullptr);
    CHECK(t.r.owner->layouts().counted(dial_ref()) == nullptr);
    CHECK(info_labels(t.r, dial_ref()).empty());
    // ...AND A DECLARATION THE NEW IMAGE SENDS COUNTS.
    t.declare(dial_declared());
    CHECK(info_labels(t.r, dial_ref()) == std::vector<std::string>{"legend", "mode", "step"});
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
    const PaneSettingCommitAnswered wrote =
        t.commit(number_at(t.read().revision, "step", 5));
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    REQUIRE(t.seat->handed.size() == handed + 1);
    CHECK(t.seat->handed.back().revision == wrote.revision);
    CHECK(find_pane_setting(t.seat->handed.back().settings, "step")->number == 5);
}

TEST_CASE("a pane's row is read by message: its revision, what its declaration takes, its pins and what it keeps") {
    RevisionRig t;
    t.open({PaneSetting{"step", {}, 4, {}}});
    const PaneSettingsAnswered shown = t.read();
    CHECK(shown.provider == kDialOffice);
    CHECK(shown.pane == kDial);
    CHECK(shown.revision == t.seat->handed.back().revision);
    CHECK(shown.rows == dial_declared_v2().rows);
    CHECK(shown.pins == std::vector<std::string>{"legend"});
    CHECK(shown.settings == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
    CHECK(shown.locks.empty());
    CHECK(shown.said.empty());
    // READING AGAIN MOVES NOTHING.
    CHECK(t.read().revision == shown.revision);

    // A LAYOUT THAT DOES NOT SEAT THE PANE: revision 0, its declaration still said, and why.
    layout_key(t.r, Act::kLayoutNew);
    const PaneSettingsAnswered away = t.read();
    CHECK(away.revision == 0);
    CHECK(away.settings.empty());
    CHECK(away.rows == dial_declared_v2().rows);
    CHECK(away.said == "test.dial/dial has no row on the live layout");
}

TEST_CASE("a commit writes one setting at the revision it read, hands it, and says how far it got") {
    RevisionRig t;
    t.open();
    const std::int64_t before = t.read().revision;
    const PaneSettingCommitAnswered wrote = t.commit(number_at(before, "step", 5));
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    CHECK(wrote.refusal.empty());
    CHECK(wrote.revision > before);
    CHECK(wrote.handed == "stored");
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 5, {}}});
    CHECK(t.r.last_notice() == "committed step of Dial = 5; stored and handed");
    CHECK(t.read().revision == wrote.revision);

    // A TEXT AMONG ITS CHOICES, AND A FLAG, EACH IN ITS OWN FIELD.
    PaneSettingCommitRequested mode;
    mode.revision = wrote.revision;
    mode.key = "mode";
    mode.text = "narrow";
    const PaneSettingCommitAnswered narrow = t.commit(mode);
    REQUIRE_MESSAGE(narrow.accepted, narrow.refusal);
    PaneSettingCommitRequested legend;
    legend.revision = narrow.revision;
    legend.key = "legend";
    legend.flag = false;
    const PaneSettingCommitAnswered off = t.commit(legend);
    REQUIRE_MESSAGE(off.accepted, off.refusal);
    CHECK(t.stored() == std::vector<PaneSetting>{legend_off(),
                                                 PaneSetting{"mode", {}, {}, std::string("narrow")},
                                                 PaneSetting{"step", {}, 5, {}}});

    // THE DEFAULT IS STORED AS ITS ABSENCE, through the same setter Info's rows use.
    const PaneSettingCommitAnswered one = t.commit(number_at(off.revision, "step", 1));
    REQUIRE_MESSAGE(one.accepted, one.refusal);
    CHECK(find_pane_setting(t.stored(), "step") == nullptr);

    // ...AND A VALUE ALREADY KEPT MINTS NOTHING AND HANDS NOTHING.
    const std::size_t handed = t.seat->handed.size();
    const PaneSettingCommitAnswered same = t.commit(number_at(one.revision, "step", 1));
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
        const PaneSettingCommitAnswered refused = t.commit(c.asked);
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
    const PaneSettingCommitAnswered late = t.commit(number_at(now, "step", 2));
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
    const PaneSettingCommitAnswered cleared = t.commit(zoom);
    REQUIRE_MESSAGE(cleared.accepted, cleared.refusal);
    CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"step", {}, 4, {}}});
    CHECK(t.seat->handed.back().settings == t.stored());
    CHECK(t.seat->handed.back().revision == cleared.revision);
    CHECK(t.r.last_notice() == "cleared zoom of Dial; stored and handed");
    // ...AND THE SAME CLEAR AGAIN IS ACCEPTED, minting and handing nothing.
    const std::size_t handed = t.seat->handed.size();
    zoom.revision = cleared.revision;
    const PaneSettingCommitAnswered again = t.commit(zoom);
    CHECK(again.accepted);
    CHECK(again.revision == cleared.revision);
    CHECK(t.seat->handed.size() == handed);
    // A ROW WHOSE PANE NOTHING OFFERS IS CLEARED TOO: no declaration is needed to take a value back.
    const PaneRef ghost{"test.ghost", "ghost"};
    stage_desk(t.r, [&](Setup& desk) {
        REQUIRE(add_pane(desk, ghost));
    });
    stage_desk(t.r, [&](Setup& desk) {
        pane_of(desk, ghost)->settings = {PaneSetting{"tone", {}, 3, {}}};
    });
    std::vector<PaneSettingsAnswered>& shown = t.editor->shown;
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
    const PaneSettingsAnswered read = t.read(/*personally=*/true);
    CHECK(read.revision == 0);
    CHECK(read.rows.empty());
    CHECK(read.settings.empty());
    CHECK(read.said ==
          "settings withheld: a pane's settings are read and written by an office, this "
          "Workshop's Terminal or a guest with `settings`");
    const PaneSettingCommitAnswered wrote = t.commit(number_at(now, "step", 2), /*personally=*/true);
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
    CHECK(t.read().said.empty());
    CHECK(t.r.w->settings_handed(dial_ref()) == "stored and handed");

    unused(at, "step", "the dial is jammed");
    REQUIRE(standing().has_value());
    CHECK(standing()->compact == "`test.dial/dial` cannot use its setting step");
    CHECK(standing()->detail == "the dial is jammed");
    CHECK(t.read().said.empty());
    CHECK(t.r.w->settings_handed(dial_ref()) == "stored and handed; it cannot use step -- the dial is jammed");
    // ...SAID PERSONALLY, IT IS NOBODY'S WORD ABOUT THE PANE.
    drive(t.r, t.seat_id, t.seat, [at](RevisionSeat& s, loom::Mail& m) {
        s.say(m, PaneSettingUnused{kDial, at, "mode", "personally"}, /*personally=*/true);
    });
    CHECK(t.read().said.empty());
    CHECK(t.r.w->settings_handed(dial_ref()) == "stored and handed; it cannot use step -- the dial is jammed");
    // ...A WORD IT CANNOT HAVE MEANT IS REFUSED ALOUD.
    unused(at, "Step", "the dial is jammed");
    CHECK(t.r.last_notice().find("Dial @test.dial: a setting it cannot use was not kept -- ") == 0);

    // A NEWER REVISION HANDED TAKES THE WORD WITH THE REVISION IT WAS ABOUT.
    const PaneSettingCommitAnswered wrote = t.commit(number_at(t.read().revision, "step", 3));
    REQUIRE(wrote.accepted);
    CHECK_FALSE(standing().has_value());
    CHECK(wrote.handed == "stored");
    // ...AND ONE ABOUT A DESK NO LONGER LIVE IS LATE TOO.
    const std::int64_t before_switch = t.seat->handed.back().revision;
    duplicate_live_layout(t.r);
    unused(before_switch, "step", "the dial is jammed");
    CHECK_FALSE(standing().has_value());
}

// ---- Office-wide settings -----------------------------------------------------------------

TEST_CASE("Workshop's own settings are read by message: what each takes, what is kept, the revision, and whether the file holds it") {
    RevisionRig t;
    t.open();
    const OfficeSettingsAnswered shown = t.read_office(kWorkshopProvider);
    CHECK(shown.office == kWorkshopProvider);
    CHECK(shown.revision > 0);
    std::vector<std::string> keys;
    for (const v2::PaneSettingRow& row : shown.rows) {
        keys.push_back(row.key);
    }
    CHECK(keys == std::vector<std::string>{
                      "workshop.pane-titles", "workshop.band-keys", "workshop.hotkeys-keys",
                      "workshop.pinned-scope", "workshop.focus.ring", "workshop.skin",
                      "workshop.completion-order", "workshop.arrange-step", "workshop.snap-reach",
                      "workshop.side-columns", "workshop.stack-rows"});
    CHECK(shown.rows == prefs_persist::workshop_office_rows());
    // THREE PINNED BY DEFAULT, the most a declaration pins, and every row sound by a pane
    // declaration's rules but its `workshop.` key.
    std::vector<std::string> pinned;
    std::vector<std::string> tags;
    for (const v2::PaneSettingRow& row : shown.rows) {
        if (row.pinned) {
            pinned.push_back(row.key);
        }
        v2::PaneSettingRow as_pane = row;
        as_pane.key = row.key.substr(std::string("workshop.").size());
        CHECK_MESSAGE(v2::pane_setting_row_problem(as_pane).empty(), row.key);
        for (const std::string& tag : row.tags) {
            if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
                tags.push_back(tag);
            }
        }
    }
    CHECK(pinned == std::vector<std::string>{"workshop.pane-titles", "workshop.pinned-scope",
                                             "workshop.skin"});
    CHECK(pinned.size() <= kMaxDefaultPins);
    CHECK(tags.size() <= kMaxDeclaredTags);
    const auto row_of = [&shown](const char* key) {
        for (const v2::PaneSettingRow& row : shown.rows) {
            if (row.key == key) {
                return row;
            }
        }
        FAIL("no row ", key);
        return v2::PaneSettingRow{};
    };
    CHECK(row_of("workshop.skin").text == std::string("classic"));
    CHECK(row_of("workshop.skin").choices ==
          std::vector<std::string>{"classic", "block", "day", "night"});
    CHECK(row_of("workshop.completion-order").text == std::string("declared"));
    CHECK(row_of("workshop.completion-order").choices ==
          std::vector<std::string>{"declared", "required-first"});
    CHECK((row_of("workshop.arrange-step").number == 4 && row_of("workshop.arrange-step").low == 2 &&
           row_of("workshop.arrange-step").high == 16));
    CHECK((row_of("workshop.snap-reach").number == 8 && row_of("workshop.snap-reach").low == 6 &&
           row_of("workshop.snap-reach").high == 11));
    CHECK((row_of("workshop.side-columns").number == 28 &&
           row_of("workshop.side-columns").low == 16 && row_of("workshop.side-columns").high == 64));
    CHECK((row_of("workshop.stack-rows").number == 9 && row_of("workshop.stack-rows").low == 5 &&
           row_of("workshop.stack-rows").high == 40));
    CHECK(shown.settings.empty());
    CHECK(shown.locks.empty());
    CHECK(shown.written == "isolated run -- no settings file");
    // READING AGAIN MOVES NOTHING; AN OFFICE NOTHING DECLARES FOR STANDS AT 0.
    CHECK(t.read_office(kWorkshopProvider).revision == shown.revision);
    CHECK(t.read_office("test.nobody").revision == 0);
    CHECK(t.read_office("test.nobody").rows.empty());
}

TEST_CASE("a commit of Workshop's own setting applies it, keeps it in the settings file, and says both") {
    TempDir dir("office-settings-workshop");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open();
    REQUIRE(t.r.session().pane_titles);
    // THE TITLES, A FLAG.
    OfficeSettingCommitRequested titles;
    titles.office = kWorkshopProvider;
    titles.revision = t.read_office(kWorkshopProvider).revision;
    titles.key = "workshop.pane-titles";
    titles.flag = false;
    const OfficeSettingCommitAnswered hidden = t.commit_office(titles);
    REQUIRE_MESSAGE(hidden.accepted, hidden.refusal);
    CHECK(hidden.handed == "applied; written");
    CHECK(hidden.revision > titles.revision);
    CHECK_FALSE(t.r.session().pane_titles);
    CHECK(t.r.last_notice() == "committed workshop.pane-titles = off of zengine.workshop; applied; written");
    prefs_persist::LoadedPrefs read = prefs_persist::load_file(t.r.host.prefs_path);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK_FALSE(read.prefs.titles_shown);
    CHECK(t.read_office(kWorkshopProvider).written == "written");
    // THE RING: plain draws one ring in the chrome's ink.
    const OfficeSettingCommitAnswered plain = t.commit_office(
        workshop_text(hidden.revision, "workshop.focus.ring", "plain"));
    REQUIRE_MESSAGE(plain.accepted, plain.refusal);
    CHECK(t.r.session().selection_ring_plain);
    // THE PIN SCOPE.
    const OfficeSettingCommitAnswered scope = t.commit_office(
        workshop_text(plain.revision, "workshop.pinned-scope", "per-layout"));
    REQUIRE_MESSAGE(scope.accepted, scope.refusal);
    read = prefs_persist::load_file(t.r.host.prefs_path);
    REQUIRE(read.outcome.accepted);
    CHECK(read.prefs.pins_per_layout);
    CHECK(read.prefs.ring_plain);
    // ...AND A CLEAR PUTS ONE BACK, kept as its absence.
    OfficeSettingCommitRequested back;
    back.office = kWorkshopProvider;
    back.revision = scope.revision;
    back.key = "workshop.pane-titles";
    back.clear = true;
    const OfficeSettingCommitAnswered cleared = t.commit_office(back);
    REQUIRE_MESSAGE(cleared.accepted, cleared.refusal);
    CHECK(t.r.session().pane_titles);
    CHECK(prefs_persist::office_value(prefs_persist::load_file(t.r.host.prefs_path).kept,
                                      kWorkshopProvider, "workshop.pane-titles") == nullptr);
    // A VALUE ALREADY KEPT MINTS NOTHING.
    const OfficeSettingCommitAnswered same = t.commit_office(
        workshop_text(cleared.revision, "workshop.focus.ring", "plain"));
    CHECK(same.accepted);
    CHECK(same.handed == "unchanged");
    CHECK(same.revision == cleared.revision);
}

TEST_CASE("an office commit is refused in words with nothing written: a stale revision, an office nothing declares for, a value not taken, a key not declared, a lock") {
    // AN EVERYWHERE LOCK ON THE RING, written in the settings file before the launch.
    TempDir dir("office-settings-refused");
    prefs_persist::Prefs locked;
    locked.locks = {prefs_persist::SettingLock{kWorkshopProvider, "workshop.focus.ring"}};
    const std::string path = dir.file("workshop-prefs.json");
    REQUIRE(prefs_persist::save_file(path, locked).accepted);
    RevisionRig t;
    t.r.host.prefs_path = path;
    t.open();
    const std::int64_t rev = t.read_office(kWorkshopProvider).revision;
    OfficeSettingCommitAnswered a = t.commit_office(workshop_text(rev - 1, "workshop.focus.ring", "plain"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "stale revision " + std::to_string(rev - 1) +
                           " -- the settings of zengine.workshop stand at " + std::to_string(rev) +
                           "; read them again");
    a = t.commit_office(workshop_text(rev, "workshop.band-keys", "tiny"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "workshop.band-keys takes one of short, full, not a text `tiny`");
    a = t.commit_office(workshop_text(rev, "workshop.nothing", "x"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "zengine.workshop declares no setting `workshop.nothing` -- a key no "
                       "declaration takes is only cleared");
    OfficeSettingCommitRequested nobody = workshop_text(0, "x", "y");
    nobody.office = "test.nobody";
    a = t.commit_office(nobody);
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "nothing declares office-wide settings for `test.nobody` here");
    CHECK_FALSE(t.r.session().selection_ring_plain);
    // THE LOCK HOLDS ITS KEY, and the file is as it was.
    a = t.commit_office(workshop_text(rev, "workshop.focus.ring", "plain"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == locked_setting_refusal("workshop.focus.ring"));
    CHECK_FALSE(t.r.session().selection_ring_plain);
    const std::vector<SettingLockShown> locks = t.read_office(kWorkshopProvider).locks;
    REQUIRE(locks.size() == 1);
    CHECK(locks[0].key == "workshop.focus.ring");
    CHECK(locks[0].scope == "everywhere");
    CHECK(prefs_persist::load_file(path).kept == locked);
}

TEST_CASE("an office declares the office-wide settings it takes, is handed what is kept, and its settings are written by message") {
    RevisionRig t;
    t.open();
    v2::PaneSettingRow face;
    face.key = "face";
    face.number = 12;
    face.low = 8;
    face.high = 32;
    face.tags = {"text"};
    face.summary = "the face size";
    drive(t.r, t.seat_id, t.seat,
          [face](RevisionSeat& s, loom::Mail& m) { s.say(m, OfficeSettingsDeclared{{face}}); });
    // HANDED AT ONCE, NOTHING KEPT, BY WORKSHOP'S OFFICE.
    REQUIRE(t.seat->office_handed.size() == 1);
    CHECK(t.seat->office_handed.back().settings.empty());
    CHECK(t.seat->authors.back() == kLayoutsRole);
    const OfficeSettingsAnswered shown = t.read_office(kDialOffice);
    CHECK(shown.rows == std::vector<v2::PaneSettingRow>{face});
    CHECK(shown.revision == t.seat->office_handed.back().revision);
    OfficeSettingCommitRequested bigger;
    bigger.office = kDialOffice;
    bigger.revision = shown.revision;
    bigger.key = "face";
    bigger.number = 15;
    const OfficeSettingCommitAnswered wrote = t.commit_office(bigger);
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    CHECK(wrote.handed == "handed; not written: isolated run -- no settings file; the change "
                          "stands for this run");
    REQUIRE(t.seat->office_handed.size() == 2);
    CHECK(t.seat->office_handed.back().revision == wrote.revision);
    CHECK(t.seat->office_handed.back().settings ==
          std::vector<PaneSetting>{PaneSetting{"face", {}, 15, {}}});
    // A VALUE OUT OF ITS RANGE IS REFUSED BY THE DECLARATION'S OWN WORDS.
    bigger.revision = wrote.revision;
    bigger.number = 99;
    const OfficeSettingCommitAnswered big = t.commit_office(bigger);
    CHECK_FALSE(big.accepted);
    CHECK(big.refusal == "face takes a whole number from 8 to 32, not a number `99`");
    // A DECLARATION REFUSED BY A PANE'S RULES IS SAID, AND THE ONE IN FORCE STANDS.
    v2::PaneSettingRow own = face;
    own.key = "workshop.face";
    drive(t.r, t.seat_id, t.seat,
          [own](RevisionSeat& s, loom::Mail& m) { s.say(m, OfficeSettingsDeclared{{own}}); });
    CHECK(t.r.last_notice().rfind("office-wide settings of test.dial: refused -- setting "
                                  "`workshop.face` is Workshop's own", 0) == 0);
    CHECK(t.read_office(kDialOffice).rows == std::vector<v2::PaneSettingRow>{face});
}

// ---- Pins ---------------------------------------------------------------------------------

TEST_CASE("a pin act pins, unpins and moves a pane's setting in the Workshop-wide list, and a reading of the row says the list") {
    TempDir dir("pins-workshop-wide");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open();
    // THE DECLARED PINS UNTIL AN ACT: legend.
    CHECK(t.read().pins == std::vector<std::string>{"legend"});
    PaneSettingPinAnswered a = t.pin("step", "pin");
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.pins == std::vector<std::string>{"legend", "step"});
    CHECK(a.scope == "workshop-wide");
    a = t.pin("step", "up");
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.pins == std::vector<std::string>{"step", "legend"});
    CHECK(t.read().pins == std::vector<std::string>{"step", "legend"});
    // UP AT THE TOP MOVES NOTHING; DOWN MOVES IT BACK.
    CHECK(t.pin("step", "up").pins == std::vector<std::string>{"step", "legend"});
    CHECK(t.pin("step", "down").pins == std::vector<std::string>{"legend", "step"});
    a = t.pin("legend", "unpin");
    REQUIRE(a.accepted);
    CHECK(a.pins == std::vector<std::string>{"step"});
    // KEPT IN WORKSHOP'S SETTINGS FILE, under the pane's target, and read back by a new run.
    const prefs_persist::LoadedPrefs read = prefs_persist::load_file(t.r.host.prefs_path);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    REQUIRE(prefs_persist::pin_list(read.kept, "test.dial/dial") != nullptr);
    CHECK(prefs_persist::pin_list(read.kept, "test.dial/dial")->keys ==
          std::vector<std::string>{"step"});
    // RESET PUTS THE LIST BACK TO THE DECLARED PINS.
    a = t.pin("", "reset");
    REQUIRE(a.accepted);
    CHECK(a.pins == std::vector<std::string>{"legend"});
    CHECK(prefs_persist::pin_list(prefs_persist::load_file(t.r.host.prefs_path).kept,
                                  "test.dial/dial") == nullptr);
}

TEST_CASE("a pin act is refused in words with nothing written: a key not declared, one pinned already, one not pinned, an act not known, past the bound") {
    RevisionRig t;
    t.open();
    PaneSettingPinAnswered a = t.pin("zoom", "pin");
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "test.dial/dial declares no setting `zoom` to pin");
    CHECK(a.pins == std::vector<std::string>{"legend"});
    CHECK(t.pin("legend", "pin").refusal == "`legend` is pinned already");
    CHECK(t.pin("step", "up").refusal == "`step` is not pinned -- pin it first");
    CHECK(t.pin("step", "unpin").refusal == "`step` is not pinned");
    CHECK(t.pin("step", "top").refusal == "`top` is not a pin act -- pin, unpin, up, down or reset");
    CHECK(t.pin("legend", "reset").refusal ==
          "`reset` names no key -- it puts the whole list back to the declared pins");
    CHECK(t.pin("step", "pin", 7).refusal ==
          "that layout is gone -- no layout of this Workshop holds id 7");
    CHECK(t.r.owner->layouts().prefs().pins.empty());
    // A GUEST WITHOUT `settings`, OR PERSONAL SPEECH, IS WITHHELD.
    const std::size_t before = t.editor->pin_acted.size();
    drive(t.r, t.editor_id, t.editor, [](RevisionSeat& s, loom::Mail& m) {
        s.say(m, PaneSettingPinRequested{kDialOffice, kDial, "step", "pin", 0}, true);
    });
    REQUIRE(t.editor->pin_acted.size() == before + 1);
    CHECK(t.editor->pin_acted.back().refusal.rfind("pins withheld: ", 0) == 0);
}

TEST_CASE("per-layout pins keep each layout's own list, a layout with none reading the Workshop-wide one") {
    RevisionRig t;
    t.open();
    REQUIRE(t.pin("step", "pin").accepted); // Workshop-wide: legend, step
    const std::int64_t rev = t.read_office(kWorkshopProvider).revision;
    REQUIRE(t.commit_office(workshop_text(rev, "workshop.pinned-scope", "per-layout")).accepted);
    // THIS LAYOUT HAS NO LIST OF ITS OWN: it reads the Workshop-wide one, and its first act copies it.
    CHECK(t.read().pins == std::vector<std::string>{"legend", "step"});
    PaneSettingPinAnswered a = t.pin("legend", "unpin");
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.scope == "per-layout");
    CHECK(a.pins == std::vector<std::string>{"step"});
    // THE LAYOUT'S OWN LIST IS ITS DESK'S; the Workshop-wide one stays the settings file's.
    CHECK(live_desk(t.r.owner->layouts().run()).pins ==
          std::vector<SetupPins>{SetupPins{dial_ref(), {"step"}}});
    CHECK(prefs_persist::pin_list(t.r.owner->layouts().prefs(), "test.dial/dial")->keys ==
          std::vector<std::string>{"legend", "step"});
    CHECK_FALSE(prefs_persist::has_layout_entries(t.r.owner->layouts().prefs()));
    // A COPY TAKES ITS OWN LIST, and an act on the copy, named by its id, moves that list alone.
    duplicate_live_layout(t.r);
    stage_desk(t.r, [](Setup& desk) { desk.name = "Copy"; });
    layout_key(t.r, Act::kLayoutPrevious);
    CHECK(t.read().pins == std::vector<std::string>{"step"});
    a = t.pin("mode", "pin", 2);
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.pins == std::vector<std::string>{"step", "mode"});
    CHECK(t.read().pins == std::vector<std::string>{"step"});
}

TEST_CASE("an everywhere lock on a pane's setting refuses the message commit and the subject's row, and every reading says it") {
    TempDir dir("pane-setting-locked");
    prefs_persist::Prefs locked;
    locked.locks = {prefs_persist::SettingLock{"test.dial/dial", "legend"}};
    const std::string path = dir.file("workshop-prefs.json");
    REQUIRE(prefs_persist::save_file(path, locked).accepted);
    RevisionRig t;
    t.r.host.prefs_path = path;
    t.open();
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    const PaneSettingsAnswered shown = t.read();
    REQUIRE(shown.locks.size() == 1);
    CHECK(shown.locks[0].key == "legend");
    CHECK(shown.locks[0].scope == "everywhere");
    // THE MESSAGE COMMIT, A CLEAR INCLUDED, IS REFUSED BY THE LOCK, nothing written.
    PaneSettingCommitRequested off;
    off.revision = shown.revision;
    off.key = "legend";
    off.flag = false;
    PaneSettingCommitAnswered a = t.commit(off);
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == locked_setting_refusal("legend"));
    PaneSettingCommitRequested clear;
    clear.revision = shown.revision;
    clear.key = "legend";
    clear.clear = true;
    CHECK(t.commit(clear).refusal == locked_setting_refusal("legend"));
    CHECK(t.stored().empty());
    // ...AND THE SUBJECT'S ROW SAYS THE LOCK IN ITS VALUE, and its write is refused in the same words.
    CHECK(info_value(t.r, dial_ref(), "legend") == "on -- locked everywhere");
    const PaneSubjectActed typed = info_commit(t.r, dial_ref(), "legend", "off");
    CHECK_FALSE(typed.accepted);
    CHECK(typed.refusal == "legend: " + locked_setting_refusal("legend"));
    CHECK(t.stored().empty());
    // A SETTING NO LOCK HOLDS IS WRITTEN AS EVER.
    CHECK(t.commit(number_at(shown.revision, "step", 4)).accepted);
}

TEST_CASE("Workshop's measures and its completion order apply when committed, are kept in the settings file, and a value outside what each takes is refused") {
    TempDir dir("office-settings-measures");
    RevisionRig t;
    const std::string path = dir.file("workshop-prefs.json");
    t.r.host.prefs_path = path;
    t.open();
    const Session& s = t.r.session();
    REQUIRE(s.arrange_step_cells == kCoarseStepCells);
    REQUIRE(s.snap_reach_px == kPaneSnapReachPx);
    REQUIRE(s.side_cols == kSideCols);
    REQUIRE(s.stack_rows == kStackRows);
    REQUIRE_FALSE(s.completion_required_first);
    std::int64_t rev = t.read_office(kWorkshopProvider).revision;

    // EACH MEASURE, APPLIED THE MOMENT IT IS COMMITTED, and kept in the file.
    struct Set {
        const char* key;
        std::int64_t value;
    };
    for (const Set& c : {Set{"workshop.arrange-step", 8}, Set{"workshop.snap-reach", 11},
                         Set{"workshop.side-columns", 40}, Set{"workshop.stack-rows", 20}}) {
        CAPTURE(c.key);
        const OfficeSettingCommitAnswered a = t.commit_office(workshop_number(rev, c.key, c.value));
        REQUIRE_MESSAGE(a.accepted, a.refusal);
        CHECK(a.handed == "applied; written");
        rev = a.revision;
    }
    CHECK(s.arrange_step_cells == 8);
    CHECK(s.snap_reach_px == 11);
    CHECK(s.side_cols == 40);
    CHECK(s.stack_rows == 20);
    // THE SCREEN TAKES THE COLUMN AND THE STACK AT ONCE: the column forty cells from the right
    // edge, and each stacked slot twenty rows tall with its gap beneath.
    const Screen sc = screen_of(s);
    CHECK(sc.side_x == sc.w - cells_px(40));
    CHECK(placement_bounds(placement::kSideRegion, 0, sc).w == cells_px(40));
    CHECK(placement_bounds(placement::kOverlayStack, 0, sc).h == cells_px(20));
    CHECK(placement_bounds(placement::kOverlayStack, 1, sc).y == sc.room_y + cells_px(21));
    CHECK(stack_capacity(sc).fallback_height == cells_px(20));
    // THE COMPLETION'S ORDER, a choice.
    const OfficeSettingCommitAnswered order =
        t.commit_office(workshop_text(rev, "workshop.completion-order", "required-first"));
    REQUIRE_MESSAGE(order.accepted, order.refusal);
    CHECK(s.completion_required_first);
    rev = order.revision;

    // READ BACK FROM THE FILE as the session spends them.
    const prefs_persist::LoadedPrefs read = prefs_persist::load_file(path);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.prefs.arrange_step_cells == 8);
    CHECK(read.prefs.snap_reach_px == 11);
    CHECK(read.prefs.side_cols == 40);
    CHECK(read.prefs.stack_rows == 20);
    CHECK(read.prefs.completion_required_first);
    const std::string kept = slurp(path);

    // A VALUE OUTSIDE WHAT EACH TAKES IS REFUSED IN ITS WORDS, nothing applied or written.
    struct Bad {
        OfficeSettingCommitRequested asked;
        std::string said;
    };
    const auto number = [rev](const char* key, std::int64_t n) {
        return workshop_number(rev, key, n);
    };
    const std::vector<Bad> bad = {
        {number("workshop.arrange-step", 1),
         "workshop.arrange-step takes a whole number from 2 to 16, not a number `1`"},
        {number("workshop.arrange-step", 17),
         "workshop.arrange-step takes a whole number from 2 to 16, not a number `17`"},
        {number("workshop.snap-reach", 5),
         "workshop.snap-reach takes a whole number from 6 to 11, not a number `5`"},
        {number("workshop.snap-reach", 12),
         "workshop.snap-reach takes a whole number from 6 to 11, not a number `12`"},
        {number("workshop.side-columns", 15),
         "workshop.side-columns takes a whole number from 16 to 64, not a number `15`"},
        {number("workshop.side-columns", 65),
         "workshop.side-columns takes a whole number from 16 to 64, not a number `65`"},
        {number("workshop.stack-rows", 4),
         "workshop.stack-rows takes a whole number from 5 to 40, not a number `4`"},
        {number("workshop.stack-rows", 41),
         "workshop.stack-rows takes a whole number from 5 to 40, not a number `41`"},
        {workshop_text(rev, "workshop.completion-order", "alphabetical"),
         "workshop.completion-order takes one of declared, required-first, not a text "
         "`alphabetical`"},
        {workshop_text(rev, "workshop.stack-rows", "tall"),
         "workshop.stack-rows takes a whole number from 5 to 40, not a text `tall`"},
    };
    for (const Bad& b : bad) {
        CAPTURE(b.said);
        const OfficeSettingCommitAnswered a = t.commit_office(b.asked);
        CHECK_FALSE(a.accepted);
        CHECK(a.refusal == b.said);
        CHECK(a.revision == rev);
    }
    CHECK(s.arrange_step_cells == 8);
    CHECK(s.stack_rows == 20);
    CHECK(slurp(path) == kept);

    // ...AND A FILE HOLDING ONE IS REFUSED WHOLE when it is read.
    prefs_persist::Prefs wrong;
    wrong.offices = {prefs_persist::OfficeSettingsKept{
        kWorkshopProvider, {PaneSetting{"workshop.stack-rows", std::nullopt, 41, std::nullopt}}}};
    const prefs_persist::LoadedPrefs refused =
        prefs_persist::from_text(prefs_persist::to_text(wrong));
    CHECK_FALSE(refused.outcome.accepted);
    CHECK(refused.outcome.refusal ==
          "workshop.stack-rows takes a whole number from 5 to 40, not a number `41`");
}

TEST_CASE("a row's `workshop.skin` is Workshop's word for its pane's area: taken on every row, kept as written, shown in Info, and never handed to the pane") {
    RevisionRig t;
    t.open();
    const PaneSettingsAnswered before = t.read();
    // A ROW TAKES IT, whatever its pane declares, and the pane is handed none of it.
    const PaneSettingCommitAnswered block = t.commit(text_at(before.revision, "workshop.skin", "block"));
    REQUIRE_MESSAGE(block.accepted, block.refusal);
    CHECK(block.handed == "stored");
    CHECK(t.stored() ==
          std::vector<PaneSetting>{PaneSetting{"workshop.skin", std::nullopt, std::nullopt,
                                               std::string("block")}});
    REQUIRE_FALSE(t.seat->handed.empty());
    CHECK(t.seat->handed.back().revision == block.revision);
    CHECK(t.seat->handed.back().settings.empty());
    const PaneSettingsAnswered shown = t.read();
    CHECK(shown.settings == t.stored());
    CHECK(shown.said.empty());
    for (const v2::PaneSettingRow& row : shown.rows) {
        CHECK(row.key != "workshop.skin"); // the pane's declaration, as the pane declared it
    }
    // INFO SHOWS IT AS A SETTING THE ROW TAKES, not a key kept for nobody.
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    CHECK(info_value(t.r, dial_ref(), "workshop.skin") == "block");

    // `classic` IS KEPT AS WRITTEN: its absence is Workshop's own skin, which may be `block`.
    const PaneSettingCommitAnswered classic = t.commit(text_at(block.revision, "workshop.skin", "classic"));
    REQUIRE_MESSAGE(classic.accepted, classic.refusal);
    CHECK(t.stored() ==
          std::vector<PaneSetting>{PaneSetting{"workshop.skin", std::nullopt, std::nullopt,
                                               std::string("classic")}});
    CHECK(t.seat->handed.back().settings.empty());
    // `day` AND `night` ARE TAKEN AS THE OTHER TWO ARE, kept as written and handed to no pane.
    std::int64_t at = classic.revision;
    for (const char* painter : {"day", "night"}) {
        CAPTURE(painter);
        const PaneSettingCommitAnswered kept = t.commit(text_at(at, "workshop.skin", painter));
        REQUIRE_MESSAGE(kept.accepted, kept.refusal);
        CHECK(t.stored() == std::vector<PaneSetting>{PaneSetting{"workshop.skin", std::nullopt,
                                                                 std::nullopt,
                                                                 std::string(painter)}});
        CHECK(t.seat->handed.back().settings.empty());
        CHECK(check_pane_settings(dial_ref(), t.stored()).accepted);
        at = kept.revision;
    }
    // A SKIN IT DOES NOT TAKE, AND ANY OTHER `workshop.` KEY, ARE REFUSED, nothing written.
    PaneSettingCommitAnswered a = t.commit(text_at(at, "workshop.skin", "plain"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "workshop.skin takes one of classic, block, day, night, not a text `plain`");
    a = t.commit(text_at(at, "workshop.text", "x"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal.find("has no counted setting `workshop.text`") != std::string::npos);
    CHECK(t.stored().size() == 1);
    // ...AND A CLEAR GIVES THE AREA BACK TO WORKSHOP'S OWN.
    PaneSettingCommitRequested clear;
    clear.revision = at;
    clear.key = "workshop.skin";
    clear.clear = true;
    const PaneSettingCommitAnswered cleared = t.commit(clear);
    REQUIRE_MESSAGE(cleared.accepted, cleared.refusal);
    CHECK(t.stored().empty());

    // THE SETUP FILE'S CHECK READS A ROW KEEPING IT, and refuses one it does not take by name.
    const PaneSetting skin_block{"workshop.skin", std::nullopt, std::nullopt, std::string("block")};
    CHECK(check_pane_settings(dial_ref(), {skin_block}).accepted);
    const Written plain = check_pane_settings(
        dial_ref(), {PaneSetting{"workshop.skin", std::nullopt, std::nullopt, std::string("plain")}});
    CHECK_FALSE(plain.accepted);
    CHECK(plain.refusal ==
          "`" + ref_text(dial_ref()) +
              "`: workshop.skin takes one of classic, block, day, night, not a text `plain`");
    CHECK(check_pane_settings(dial_ref(), {PaneSetting{"workshop.skin", std::nullopt, 1,
                                                       std::nullopt}})
              .refusal.find(
                  "workshop.skin takes one of classic, block, day, night, not a number `1`") !=
          std::string::npos);
}

namespace {

/// RENAME THE LIVE LAYOUT AS A WEAVER DOES: a double-click on its tab, the name typed, then Return.
void rename_live(PaneRig& r, const std::string& name) {
    open_rename_on_live_tab(r);
    REQUIRE(r.session().setup.naming.open);
    for (int guard = 0; guard < 64 && !r.session().setup.naming.line.empty(); ++guard) {
        r.key(input::scan::kBackspace);
    }
    for (const char c : name) {
        r.text(std::string(1, c));
    }
    r.key(input::scan::kReturn);
    REQUIRE(r.session().setup.active.name == name);
}

/// The dial's row in one layout of a reading of every layout.
const LayoutPaneSettings* dial_in(const LayoutSettings& layout) {
    for (const LayoutPaneSettings& p : layout.panes) {
        if (p.provider == kDialOffice && p.pane == kDial) {
            return &p;
        }
    }
    return nullptr;
}

PaneSetting step_of(std::int64_t n) { return PaneSetting{"step", std::nullopt, n, std::nullopt}; }

constexpr const char* kWeavers =
    "unlocking is the weaver's -- from the Terminal, or from a pane on the weaver's own current act";

} // namespace

TEST_CASE("a lock in one layout freezes its row there: both commit doors refuse it, every reading says it, and another layout still writes it") {
    RevisionRig t;
    t.open();
    duplicate_live_layout(t.r);
    rename_live(t.r, "Layout 2");
    layout_key(t.r, Act::kLayoutPrevious);
    REQUIRE(t.r.session().setup.active_at == 0);
    const std::string first = t.r.session().setup.active.name;
    REQUIRE(first != "Layout 2");
    PaneSettingLockAnswered a = t.lock(dial_lock("legend", "lock", "layout"));
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.written == std::vector<std::string>{first});
    // THE LOCK IS THE LAYOUT'S OWN, in its desk; the settings file keeps none for a layout.
    CHECK(live_desk(t.r.owner->layouts().run()).locks ==
          std::vector<SetupLock>{SetupLock{dial_ref(), "legend"}});
    CHECK_FALSE(prefs_persist::has_layout_entries(t.r.owner->layouts().prefs()));
    // EVERY READING SAYS THE LOCK, AND THAT IT HOLDS IN THIS LAYOUT.
    const PaneSettingsAnswered shown = t.read();
    REQUIRE(shown.locks.size() == 1);
    CHECK(shown.locks[0].key == "legend");
    CHECK(shown.locks[0].scope == "layout");
    REQUIRE(hand_inspect(t.r, dial_ref()).accepted);
    CHECK(info_value(t.r, dial_ref(), "legend") == "on -- locked in this layout");
    // BOTH COMMIT DOORS REFUSE IT, A CLEAR INCLUDED, and nothing is written.
    PaneSettingCommitRequested off;
    off.revision = shown.revision;
    off.key = "legend";
    off.flag = false;
    CHECK(t.commit(off).refusal == locked_setting_refusal("legend"));
    const PaneSubjectActed typed = info_commit(t.r, dial_ref(), "legend", "off");
    CHECK_FALSE(typed.accepted);
    CHECK(typed.refusal == "legend: " + locked_setting_refusal("legend"));
    CHECK(t.stored().empty());
    // THE SAME LOCK TWICE IS REFUSED.
    a = t.lock(dial_lock("legend", "lock", "layout"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "`legend` of test.dial/dial is locked in layout " +
                           quoted_setup_name(first) + " already");
    // THE OTHER LAYOUT'S ROW IS ITS OWN: no lock is read there, and the commit writes.
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.r.session().setup.active_at == 1);
    const PaneSettingsAnswered there = t.read();
    CHECK(there.locks.empty());
    off.revision = there.revision;
    const PaneSettingCommitAnswered wrote = t.commit(off);
    REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
    CHECK(t.stored() == std::vector<PaneSetting>{legend_off()});
    const LayoutSettingsAnswered all = t.layouts();
    REQUIRE(all.layouts.size() == 2);
    REQUIRE(dial_in(all.layouts[0]) != nullptr);
    REQUIRE(dial_in(all.layouts[1]) != nullptr);
    CHECK(dial_in(all.layouts[0])->locked == std::vector<std::string>{"legend"});
    CHECK(dial_in(all.layouts[1])->locked.empty());
}

TEST_CASE("an everywhere lock pins the value its layout holds into every layout that seats the pane, and a pane seated later wears it") {
    TempDir dir("pane-setting-everywhere");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open({step_of(4)});
    const std::string first = t.r.session().setup.active.name;
    duplicate_live_layout(t.r);
    rename_live(t.r, "Layout 2");
    REQUIRE(t.commit(number_at(t.read().revision, "step", 7)).accepted);
    layout_key(t.r, Act::kLayoutNew);
    REQUIRE(t.r.session().setup.active_at == 2);
    REQUIRE_FALSE(has_pane(t.r.session().setup.active, dial_ref()));
    // LOCKED FROM THE FIRST LAYOUT, NAMED BY ITS PLACE: its value goes to every row that seats the dial.
    PaneSettingLockAnswered a = t.lock(dial_lock("step", "lock", "everywhere", 1));
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.written == std::vector<std::string>{first, "Layout 2"});
    CHECK(dial_settings_at(t.r, 0) == std::vector<PaneSetting>{step_of(4)});
    CHECK(dial_settings_at(t.r, 1) == std::vector<PaneSetting>{step_of(4)});
    const SettingLock* kept = prefs_persist::lock_on(t.r.owner->layouts().prefs(), "test.dial/dial", "step");
    REQUIRE(kept != nullptr);
    CHECK(kept->number == 4);
    CHECK_FALSE(kept->clear);
    const prefs_persist::LoadedPrefs file = prefs_persist::load_file(t.r.host.prefs_path);
    REQUIRE_MESSAGE(file.outcome.accepted, file.outcome.refusal);
    CHECK(file.kept.locks == t.r.owner->layouts().prefs().locks);
    // A PANE SEATED LATER, IN A LAYOUT THAT HELD NONE, WEARS THE PINNED VALUE.
    REQUIRE(seat_pane(t.r, dial_ref()).refusal.empty());
    CHECK(t.stored() == std::vector<PaneSetting>{step_of(4)});
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{step_of(4)});
    const PaneSettingsAnswered shown = t.read();
    REQUIRE(shown.locks.size() == 1);
    CHECK(shown.locks[0].scope == "everywhere");
    CHECK(t.commit(number_at(shown.revision, "step", 2)).refusal == locked_setting_refusal("step"));
    // A KEY AT ITS DEFAULT IS PINNED TO ITS DEFAULT, and a row that kept another value loses it.
    stage_run(t.r, [](SetupState& run) {
        pane_of(run.shelved[0].desk, dial_ref())->settings = {
            PaneSetting{"mode", std::nullopt, std::nullopt, std::string("narrow")}, step_of(4)};
    });
    a = t.lock(dial_lock("mode", "lock", "everywhere"));
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(prefs_persist::lock_on(t.r.owner->layouts().prefs(), "test.dial/dial", "mode")->clear);
    CHECK(dial_settings_at(t.r, 0) == std::vector<PaneSetting>{step_of(4)});
    // A LAYOUT'S OWN LOCK ON A KEY LOCKED EVERYWHERE IS REFUSED.
    a = t.lock(dial_lock("step", "lock", "layout"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == "`step` of test.dial/dial is locked everywhere already");
    // ...AND AN EVERYWHERE LOCK TAKES THE PLACE OF EACH LAYOUT'S OWN ON ITS KEY.
    REQUIRE(t.lock(dial_lock("legend", "lock", "layout")).accepted);
    REQUIRE(t.lock(dial_lock("legend", "lock", "everywhere")).accepted);
    for (const SettingLock& l : t.r.owner->layouts().prefs().locks) {
        CHECK(l.target.find('@') == std::string::npos);
    }
    for (const Layout& l : layout_run(t.r.owner->layouts().run())) {
        CHECK(l.desk.locks.empty());
    }
}

TEST_CASE("only the weaver unlocks: a pane on the weaver's current act does, and an office's ask, a forged number and a stale act are refused") {
    RevisionRig t;
    t.open();
    REQUIRE(t.lock(dial_lock("legend", "lock", "everywhere")).accepted);
    // AN OFFICE ASKING ON NO ACT OF THE WEAVER'S.
    PaneSettingLockAnswered a = t.lock(dial_lock("legend", "unlock", "everywhere"));
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal == kWeavers);
    REQUIRE(prefs_persist::lock_on(t.r.owner->layouts().prefs(), "test.dial/dial", "legend") != nullptr);
    // THE WEAVER PRESSES THE DIAL: the press's correlation names the weaver's current act.
    press_body(t.r, t.row()->kind);
    REQUIRE_FALSE(t.seat->press_asks.empty());
    const auto act = static_cast<std::int64_t>(t.seat->press_asks.back());
    REQUIRE(act != 0);
    // A NUMBER THE WEAVER'S HAND NEVER SENT, AND ANOTHER OFFICE NAMING THE DIAL'S ACT, ARE REFUSED.
    CHECK(t.lock(dial_lock("legend", "unlock", "everywhere", 0, act + 7), true).refusal == kWeavers);
    CHECK(t.lock(dial_lock("legend", "unlock", "everywhere", 0, act)).refusal == kWeavers);
    REQUIRE(prefs_persist::lock_on(t.r.owner->layouts().prefs(), "test.dial/dial", "legend") != nullptr);
    // THE DIAL, ON THE WEAVER'S CURRENT ACT, UNLOCKS; a commit writes the key again.
    a = t.lock(dial_lock("legend", "unlock", "everywhere", 0, act), true);
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(prefs_persist::lock_on(t.r.owner->layouts().prefs(), "test.dial/dial", "legend") == nullptr);
    PaneSettingCommitRequested off;
    off.revision = t.read().revision;
    off.key = "legend";
    off.flag = false;
    CHECK(t.commit(off).accepted);
    // AN ACT THE WEAVER HAS SINCE MOVED ON FROM IS STALE.
    REQUIRE(t.lock(dial_lock("legend", "lock", "layout")).accepted);
    press_body(t.r, t.row()->kind);
    const auto stale = static_cast<std::int64_t>(t.seat->press_asks.back());
    press_body(t.r, t.row()->kind);
    CHECK(t.lock(dial_lock("legend", "unlock", "layout", 0, stale), true).refusal == kWeavers);
    // UNLOCKED WHERE IT HOLDS, OR REFUSED NAMING WHERE IT DOES.
    const auto now = static_cast<std::int64_t>(t.seat->press_asks.back());
    a = t.lock(dial_lock("legend", "unlock", "everywhere", 0, now), true);
    CHECK(a.refusal == "`legend` of test.dial/dial is not locked everywhere");
    a = t.lock(dial_lock("legend", "unlock", "layout", 0, now), true);
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.written == std::vector<std::string>{t.r.session().setup.active.name});
    CHECK(t.r.owner->layouts().prefs().locks.empty());
}

TEST_CASE("a lock act is refused in words with nothing changed: an act or a scope not known, a target naming its layout, no such layout, a key nothing takes, a pane its layout does not seat") {
    RevisionRig t;
    t.open();
    CHECK(t.lock(dial_lock("legend", "freeze", "layout")).refusal ==
          "`freeze` is not a lock act -- lock or unlock");
    CHECK(t.lock(dial_lock("legend", "lock", "here")).refusal ==
          "`here` is not where a lock holds -- layout or everywhere");
    PaneSettingLockRequested at = dial_lock("legend", "lock", "layout");
    at.target = "test.dial/dial@Elsewhere";
    CHECK(t.lock(at).refusal == "a lock names its layout by `layout`, never in its target");
    CHECK(t.lock(dial_lock("legend", "lock", "layout", 3)).refusal ==
          "that layout is gone -- no layout of this Workshop holds id 3");
    CHECK(t.lock(dial_lock("nothing", "lock", "layout")).refusal ==
          "test.dial/dial neither declares nor keeps `nothing` in layout " +
              quoted_setup_name(t.r.session().setup.active.name));
    PaneSettingLockRequested away = dial_lock("legend", "lock", "layout");
    away.target = "test.dial/other";
    CHECK(t.lock(away).refusal == "test.dial/other neither declares nor keeps `legend` in layout " +
                                      quoted_setup_name(t.r.session().setup.active.name));
    away.key = kWorkshopSkinKey;
    CHECK(t.lock(away).refusal == "test.dial/other is not in layout " +
                                      quoted_setup_name(t.r.session().setup.active.name) +
                                      " -- a layout's lock holds its row");
    CHECK(t.r.owner->layouts().prefs().locks.empty());
    // ...AND TO AN ASKER THE SETTINGS DOORS DO NOT SERVE.
    PaneSettingLockAnswered withheld;
    const std::size_t before = t.editor->lock_acted.size();
    drive(t.r, t.editor_id, t.editor, [](RevisionSeat& s, loom::Mail& m) {
        s.say(m, dial_lock("legend", "lock", "layout"), true);
    });
    REQUIRE(t.editor->lock_acted.size() == before + 1);
    withheld = t.editor->lock_acted.back();
    CHECK_FALSE(withheld.accepted);
    CHECK(withheld.refusal.rfind("lock withheld: ", 0) == 0);
    CHECK(t.r.owner->layouts().prefs().locks.empty());
}

TEST_CASE("an office-wide setting is locked everywhere alone, and its lock freezes the office's commit") {
    TempDir dir("office-setting-lock");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open();
    PaneSettingLockRequested ring{kWorkshopProvider, "workshop.focus.ring", "lock", "layout", 0, 0};
    CHECK(t.lock(ring).refusal ==
          "an office-wide setting has one value in every layout -- lock it everywhere");
    ring.scope = "everywhere";
    PaneSettingLockAnswered a = t.lock(ring);
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(a.written.empty());
    const OfficeSettingsAnswered shown = t.read_office(kWorkshopProvider);
    REQUIRE(shown.locks.size() == 1);
    CHECK(shown.locks[0].key == "workshop.focus.ring");
    const OfficeSettingCommitAnswered refused =
        t.commit_office(workshop_text(shown.revision, "workshop.focus.ring", "plain"));
    CHECK_FALSE(refused.accepted);
    CHECK(refused.refusal == locked_setting_refusal("workshop.focus.ring"));
    CHECK_FALSE(t.r.session().selection_ring_plain);
    CHECK(t.lock(ring).refusal == "`workshop.focus.ring` of zengine.workshop is locked already");
    ring.key = "workshop.nothing";
    CHECK(t.lock(ring).refusal == "zengine.workshop declares no setting `workshop.nothing` to lock");
    PaneSettingLockRequested nobody{"test.nobody", "anything", "lock", "everywhere", 0, 0};
    CHECK(t.lock(nobody).refusal == "nothing declares office-wide settings for `test.nobody` here");
    const prefs_persist::LoadedPrefs file = prefs_persist::load_file(t.r.host.prefs_path);
    REQUIRE_MESSAGE(file.outcome.accepted, file.outcome.refusal);
    REQUIRE(file.kept.locks.size() == 1);
    CHECK(file.kept.locks[0].target == kWorkshopProvider);
    // A FILE ANOTHER HAND CHANGED SINCE IS NEVER WRITTEN OVER, so the act is refused, nothing changed.
    REQUIRE(prefs_persist::save_file(t.r.host.prefs_path, prefs_persist::Prefs{}).accepted);
    ring.key = kWorkshopSkinKey;
    a = t.lock(ring);
    CHECK_FALSE(a.accepted);
    CHECK(a.refusal.rfind("nothing changed: Workshop's settings file was not written -- the "
                          "settings file changed since this Workshop read it",
                          0) == 0);
    REQUIRE(t.r.owner->layouts().prefs().locks.size() == 1);
    CHECK(t.r.owner->layouts().prefs().locks[0].key == "workshop.focus.ring");
}

TEST_CASE("a version-3 settings file keeps layouts' locks, pins and baselines by name, spaces and all, each judged; version 4 keeps none") {
    prefs_persist::Prefs p;
    p.pins = {prefs_persist::PinList{"test.dial/dial@Layout 2", {"step"}}};
    prefs_persist::put_lock(p, SettingLock{"test.dial/dial@Layout 2", "legend"});
    prefs_persist::put_lock(p, SettingLock{"test.dial/dial", "step", std::nullopt, 4});
    REQUIRE(prefs_persist::put_office_value(
        p, "@Layout 2", "fingerprint", PaneSetting{"fingerprint", std::nullopt, 42, std::nullopt}));
    REQUIRE(prefs_persist::put_office_value(
        p, "@Layout 2", "theme",
        PaneSetting{"theme", std::nullopt, std::nullopt, std::string("evening")}));
    CHECK(prefs_persist::has_layout_entries(p));
    CHECK(prefs_persist::kept_problem(p, true).empty());
    // VERSION 4 KEEPS WHAT HOLDS IN EVERY LAYOUT: a layout's own are its desk's.
    CHECK(prefs_persist::kept_problem(p).find("a layout's pins, locks and baseline are its own") !=
          std::string::npos);
    TempDir dir("prefs-layout-entries");
    const std::string path = dir.file("workshop-prefs.json");
    REQUIRE(prefs_persist::save_file(path, p).accepted);
    // CARRIED UNCHANGED, IN THE VERSION THAT KEEPS THEM, until a session holds them.
    CHECK(slurp(path).find("\"version\":3") != std::string::npos);
    const prefs_persist::LoadedPrefs read = prefs_persist::load_file(path);
    REQUIRE_MESSAGE(read.outcome.accepted, read.outcome.refusal);
    CHECK(read.read_version == 3);
    CHECK(read.kept == p);
    CHECK(prefs_persist::layout_theme_of(read.kept, "Layout 2") == LayoutTheme{"evening", 42});
    CHECK(prefs_persist::layout_theme_of(read.kept, "Layout 3").name.empty());
    // A BASELINE IS NOT AN OFFICE: it counts toward neither office bound.
    CHECK(prefs_persist::office_count(p) == 0);
    CHECK(prefs_persist::kept_count(p) == 0);
    // EACH FORM IS JUDGED: a layout's lock holds no value and names a pane; a layout's name is one a
    // layout takes; a baseline keeps a theme's name and its fingerprint alone.
    prefs_persist::Prefs bad = p;
    for (SettingLock& l : bad.locks) {
        l.flag = l.target.find('@') != std::string::npos ? std::optional<bool>(true) : l.flag;
    }
    CHECK(prefs_persist::kept_problem(bad, true).find("a layout's lock freezes its row") !=
          std::string::npos);
    bad = p;
    prefs_persist::put_lock(bad, SettingLock{"test.dial@Layout 2", "legend"});
    CHECK(prefs_persist::kept_problem(bad, true).find("names a layout but no pane") !=
          std::string::npos);
    bad = p;
    bad.pins[0].target = "test.dial/dial@" + std::string(40, 'x');
    CHECK(prefs_persist::kept_problem(bad, true).find("names its layout wrongly") !=
          std::string::npos);
    bad = p;
    REQUIRE(prefs_persist::put_office_value(bad, "@Layout 2", "theme",
                                            PaneSetting{"theme", std::nullopt, std::nullopt,
                                                        std::string("Evening!")}));
    CHECK(prefs_persist::kept_problem(bad, true).find("a baseline's theme name") !=
          std::string::npos);
    // WITHOUT THEM THE FILE IS VERSION 4: what holds in every layout, alone.
    const prefs_persist::Prefs wide = prefs_persist::without_layout_entries(p);
    CHECK(prefs_persist::to_text(wide).find("\"version\":4") != std::string::npos);
    CHECK(wide.locks.size() == 1);
    CHECK(wide.pins.empty());
    CHECK(wide.offices.empty());
}

TEST_CASE("what a layout keeps for itself is its desk's: a rename keeps it, a copy takes its own, a removal takes it away") {
    TempDir dir("layout-entries-follow");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open();
    duplicate_live_layout(t.r);
    rename_live(t.r, "Layout 2");
    REQUIRE(t.lock(dial_lock("legend", "lock", "layout")).accepted);
    SettingsWriteRequested b;
    b.revision = t.layouts().revision;
    b.baselines = {LayoutThemeWrite{0, LayoutTheme{"evening", 7}}};
    REQUIRE(t.batch(b).accepted);
    // THE SETTINGS FILE KEEPS NONE OF IT.
    CHECK_FALSE(prefs_persist::has_layout_entries(t.r.owner->layouts().prefs()));
    rename_live(t.r, "Night Desk");
    const std::vector<SetupLock> legend{SetupLock{dial_ref(), "legend"}};
    CHECK(live_desk(t.r.owner->layouts().run()).name == "Night Desk");
    CHECK(live_desk(t.r.owner->layouts().run()).locks == legend);
    CHECK(live_desk(t.r.owner->layouts().run()).theme == LayoutTheme{"evening", 7});
    CHECK(t.layouts().layouts[1].theme == LayoutTheme{"evening", 7});
    CHECK(t.read().locks.size() == 1);
    // A COPY TAKES ITS OWN, and a lock on one is a lock on that one alone.
    duplicate_live_layout(t.r);
    REQUIRE(layout_count(t.r.session().setup) == 3);
    CHECK(live_desk(t.r.owner->layouts().run()).locks == legend);
    CHECK(live_desk(t.r.owner->layouts().run()).theme == LayoutTheme{"evening", 7});
    // A REMOVED LAYOUT TAKES WHAT IT KEPT; the one it was copied from keeps its own.
    layout_key(t.r, Act::kLayoutRemove);
    REQUIRE(layout_count(t.r.session().setup) == 2);
    std::size_t locked = 0;
    for (const Layout& l : layout_run(t.r.owner->layouts().run())) {
        locked += l.desk.locks.size();
    }
    CHECK(locked == 1);
}

TEST_CASE("every layout is read at once: each row with its locks, what each pane and office takes, the everywhere locks, the live one and one revision") {
    RevisionRig t;
    t.open({legend_off()});
    duplicate_live_layout(t.r);
    rename_live(t.r, "Layout 2");
    REQUIRE(t.lock(dial_lock("step", "lock", "layout")).accepted);
    REQUIRE(t.lock(dial_lock("mode", "lock", "everywhere")).accepted);
    const LayoutSettingsAnswered s = t.layouts();
    CHECK(s.revision > 0);
    CHECK(s.active == 1);
    REQUIRE(s.layouts.size() == 2);
    CHECK(s.layouts[0].position == 0);
    CHECK(s.layouts[1].position == 1);
    CHECK(s.layouts[1].name == "Layout 2");
    CHECK(s.layouts[1].theme.name.empty());
    REQUIRE(dial_in(s.layouts[0]) != nullptr);
    REQUIRE(dial_in(s.layouts[1]) != nullptr);
    CHECK(dial_in(s.layouts[0])->settings == std::vector<PaneSetting>{legend_off()});
    CHECK(dial_in(s.layouts[0])->locked == std::vector<std::string>{"mode"});
    CHECK(dial_in(s.layouts[1])->locked == std::vector<std::string>{"mode", "step"});
    // WHAT EACH PANE TAKES, WORKSHOP'S WORD FOR ITS AREA FIRST, AND WHAT EACH OFFICE TAKES.
    const DeclaredSettings* dial = nullptr;
    const DeclaredSettings* workshop = nullptr;
    for (const DeclaredSettings& d : s.declared) {
        dial = d.provider == kDialOffice && d.pane == kDial ? &d : dial;
        workshop = d.provider == kWorkshopProvider && d.pane.empty() ? &d : workshop;
    }
    REQUIRE(dial != nullptr);
    REQUIRE(dial->rows.size() == 4);
    CHECK(dial->rows[0].key == kWorkshopSkinKey);
    CHECK(dial->rows[1].key == "legend");
    REQUIRE(workshop != nullptr);
    CHECK(workshop->rows == prefs_persist::workshop_office_rows());
    REQUIRE(s.everywhere.size() == 1);
    CHECK(s.everywhere[0].key == "mode");
    CHECK(s.everywhere[0].clear);
    // THE SAME READING STANDS AT THE SAME REVISION; anything it says moving moves it.
    CHECK(t.layouts().revision == s.revision);
    PaneSettingCommitRequested on;
    on.revision = t.read().revision;
    on.key = "legend";
    on.flag = true;
    REQUIRE(t.commit(on).accepted);
    const std::int64_t moved = t.layouts().revision;
    CHECK(moved > s.revision);
    layout_key(t.r, Act::kLayoutPrevious);
    CHECK(t.layouts().revision > moved);
    // ...AND TO AN ASKER THE SETTINGS DOORS DO NOT SERVE, NOTHING, at no revision.
    const LayoutSettingsAnswered none = t.layouts(true);
    CHECK(none.revision == 0);
    CHECK(none.layouts.empty());
    CHECK(none.declared.empty());
}

TEST_CASE("a batch is judged whole at its revision: a stale one, a value not taken, a layout not held or a bad baseline writes nothing") {
    TempDir dir("batch-refused");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open();
    duplicate_live_layout(t.r);
    rename_live(t.r, "Layout 2");
    const std::int64_t rev = t.layouts().revision;
    const prefs_persist::Prefs kept = t.r.owner->layouts().prefs();
    SettingsWriteRequested b;
    b.revision = rev + 5;
    b.writes = {number_write(1, "test.dial/dial", "step", 3)};
    SettingsWriteAnswered w = t.batch(b);
    CHECK_FALSE(w.accepted);
    CHECK(w.refusal == "stale revision " + std::to_string(rev + 5) + " -- the layouts stand at " +
                           std::to_string(rev) + "; read them again");
    CHECK(w.revision == rev);
    b.revision = rev;
    // A SOUND WRITE BEFORE A VALUE ITS SETTING DOES NOT TAKE IS NOT WRITTEN EITHER.
    b.writes = {number_write(1, "test.dial/dial", "step", 3),
                number_write(2, "test.dial/dial", "step", 12)};
    w = t.batch(b);
    CHECK_FALSE(w.accepted);
    CHECK(w.refusal.rfind("write 1: ", 0) == 0);
    b.writes = {number_write(1, "test.dial/dial", "step", 3),
                number_write(5, "test.dial/dial", "step", 3)};
    CHECK(t.batch(b).refusal ==
          "write 1: that layout is gone -- no layout of this Workshop holds id 5");
    b.writes = {number_write(1, "test.dial/dial", "step", 3),
                text_write(0, kWorkshopProvider, "workshop.focus.ring", "loud")};
    CHECK(t.batch(b).refusal.rfind("write 1: ", 0) == 0);
    b.writes = {number_write(1, "test.dial/dial@Layout 2", "step", 3)};
    CHECK(t.batch(b).refusal ==
          "write 0: a write names its layout by `layout`, never in its target");
    b.writes = {flag_write(1, "test.dial/dial", "legend", true)};
    b.writes[0].clear = true;
    CHECK(t.batch(b).refusal == "write 0: a clear takes no value -- name the key alone");
    b.writes = {number_write(1, "test.dial/dial", "step", 3)};
    b.baselines = {LayoutThemeWrite{1, LayoutTheme{"Evening!", 3}}};
    CHECK(t.batch(b).refusal.rfind("baseline 0: a baseline's theme name", 0) == 0);
    b.baselines = {LayoutThemeWrite{1, LayoutTheme{"", 3}}};
    CHECK(t.batch(b).refusal == "baseline 0: a baseline of no theme has no fingerprint");
    // NOTHING WAS WRITTEN, AND THE LAYOUTS STAND WHERE THEY WERE READ.
    CHECK(dial_settings_at(t.r, 0).empty());
    CHECK(dial_settings_at(t.r, 1).empty());
    CHECK(t.r.owner->layouts().prefs() == kept);
    CHECK_FALSE(t.r.session().selection_ring_plain);
    CHECK(t.layouts().revision == rev);
    // ...AND TO AN ASKER THE SETTINGS DOORS DO NOT SERVE.
    w = t.batch(b, true);
    CHECK_FALSE(w.accepted);
    CHECK(w.refusal.rfind("settings writes withheld: ", 0) == 0);
    CHECK(w.revision == 0);
    // A SETTINGS FILE ANOTHER HAND WROTE SINCE IS NEVER WRITTEN OVER: a batch writing an
    // office-wide setting is refused whole, and the layout's baseline beside it is not written.
    REQUIRE(prefs_persist::save_file(t.r.host.prefs_path, prefs_persist::Prefs{}).accepted);
    b.writes = {text_write(0, kWorkshopProvider, "workshop.focus.ring", "plain")};
    b.baselines = {LayoutThemeWrite{1, LayoutTheme{"evening", 3}}};
    w = t.batch(b);
    CHECK_FALSE(w.accepted);
    CHECK(w.refusal.rfind("nothing written: Workshop's settings file was not written -- ", 0) == 0);
    CHECK(dial_settings_at(t.r, 0).empty());
    CHECK(t.r.owner->layouts().prefs() == kept);
}

TEST_CASE("a batch writes what it can across layouts, says each write's outcome, keeps baselines, and hands the live rows and offices it moved") {
    TempDir dir("batch-written");
    RevisionRig t;
    t.r.host.prefs_path = dir.file("workshop-prefs.json");
    t.open();
    const std::string first = t.r.session().setup.active.name;
    duplicate_live_layout(t.r);
    rename_live(t.r, "Layout 2");
    REQUIRE(t.lock(dial_lock("legend", "lock", "layout", 1)).accepted);
    const std::int64_t rev = t.layouts().revision;
    SettingsWriteRequested b;
    b.revision = rev;
    b.cause = "theme evening";
    b.writes = {number_write(2, "test.dial/dial", "step", 6),
                flag_write(1, "test.dial/dial", "legend", false),
                text_write(2, "test.dial/dial", "nothing", "x"),
                number_write(1, "test.dial/other", "step", 2),
                number_write(0, "test.dial/dial", "step", 6),
                text_write(0, kWorkshopProvider, "workshop.focus.ring", "plain"),
                text_write(1, "test.dial/dial", "mode", "narrow")};
    b.baselines = {LayoutThemeWrite{1, LayoutTheme{"evening", 42}}};
    const SettingsWriteAnswered w = t.batch(b);
    REQUIRE_MESSAGE(w.accepted, w.refusal);
    REQUIRE(w.results.size() == 7);
    std::vector<std::string> outcomes;
    for (const SettingWriteResult& r : w.results) {
        outcomes.push_back(r.outcome);
    }
    CHECK(outcomes == std::vector<std::string>{"written", "locked", "not-taken", "not-in-layout",
                                               "unchanged", "written", "written"});
    CHECK(w.results[1].said == locked_setting_refusal("legend"));
    CHECK(w.results[3].said == "test.dial/other is not in layout " + quoted_setup_name(first));
    CHECK(w.revision > rev);
    CHECK(w.handed == std::vector<std::string>{"test.dial/dial", kWorkshopProvider});
    // WRITTEN WHERE EACH WAS ASKED, the live row handed, Workshop's own applied.
    CHECK(dial_settings_at(t.r, 1) == std::vector<PaneSetting>{step_of(6)});
    CHECK(dial_settings_at(t.r, 0) ==
          std::vector<PaneSetting>{
              PaneSetting{"mode", std::nullopt, std::nullopt, std::string("narrow")}});
    CHECK(t.seat->handed.back().settings == std::vector<PaneSetting>{step_of(6)});
    CHECK(t.r.session().selection_ring_plain);
    CHECK(t.r.last_notice() == "theme evening: 3 of 7 written, baselines set");
    // WORKSHOP'S OWN IS KEPT IN THE SETTINGS FILE, AND THE BASELINE IN ITS LAYOUT.
    CHECK(layout_at(t.r.owner->layouts().run(), 0).theme == LayoutTheme{"evening", 42});
    CHECK_FALSE(prefs_persist::has_layout_entries(t.r.owner->layouts().prefs()));
    const prefs_persist::LoadedPrefs file = prefs_persist::load_file(t.r.host.prefs_path);
    REQUIRE_MESSAGE(file.outcome.accepted, file.outcome.refusal);
    CHECK(file.kept == t.r.owner->layouts().prefs());
    CHECK(file.prefs.ring_plain);
    const LayoutSettingsAnswered after = t.layouts();
    CHECK(after.revision == w.revision);
    CHECK(after.layouts[0].theme == LayoutTheme{"evening", 42});
    // A BASELINE OF NO THEME REMOVES IT; a batch that moves nothing mints nothing.
    SettingsWriteRequested gone;
    gone.revision = after.revision;
    gone.baselines = {LayoutThemeWrite{1, LayoutTheme{}}};
    const SettingsWriteAnswered removed = t.batch(gone);
    REQUIRE_MESSAGE(removed.accepted, removed.refusal);
    CHECK(layout_at(t.r.owner->layouts().run(), 0).theme.name.empty());
    SettingsWriteRequested same;
    same.revision = removed.revision;
    same.writes = {number_write(2, "test.dial/dial", "step", 6)};
    const SettingsWriteAnswered unchanged = t.batch(same);
    REQUIRE(unchanged.accepted);
    CHECK(unchanged.results[0].outcome == "unchanged");
    CHECK(unchanged.revision == removed.revision);
    CHECK(unchanged.handed.empty());
}

TEST_CASE("a layout is named by its id: a lock by id lands on it after it moved, and two layouts of one name keep their own") {
    // ⚔ MUTATION: an id read as the layout's place -- after the move the lock lands on the copy
    // now standing where the first layout stood.
    RevisionRig t;
    t.open();
    duplicate_live_layout(t.r);
    REQUIRE(layout_count(t.r.session().setup) == 2);
    const auto id_at = [&t](std::size_t place) {
        return layout_run(t.r.owner->layouts().run())[place].id;
    };
    const auto locks_of = [&t](std::int64_t id) {
        for (const Layout& l : layout_run(t.r.owner->layouts().run())) {
            if (l.id == id) {
                return l.desk.locks;
            }
        }
        FAIL("no layout holds id " << id);
        return std::vector<SetupLock>{};
    };
    const std::int64_t first = id_at(0);
    const std::int64_t copy = id_at(1);
    REQUIRE(first != copy);
    // THE COPY, LIVE, MOVES IN FRONT: the first layout stands second now, its id its own.
    tab_menu_act(t.r, "layout.move-left", "Order");
    REQUIRE(id_at(0) == copy);
    REQUIRE(id_at(1) == first);
    PaneSettingLockAnswered a = t.lock(dial_lock("legend", "lock", "layout", first));
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(locks_of(first) == std::vector<SetupLock>{SetupLock{dial_ref(), "legend"}});
    CHECK(locks_of(copy).empty());
    // TWO LAYOUTS OF ONE NAME KEEP APART: each is its id, never its name.
    rename_live(t.r, layout_run(t.r.owner->layouts().run())[1].desk.name);
    REQUIRE(layout_run(t.r.owner->layouts().run())[0].desk.name ==
            layout_run(t.r.owner->layouts().run())[1].desk.name);
    a = t.lock(dial_lock("step", "lock", "layout", copy));
    REQUIRE_MESSAGE(a.accepted, a.refusal);
    CHECK(locks_of(first) == std::vector<SetupLock>{SetupLock{dial_ref(), "legend"}});
    CHECK(locks_of(copy) == std::vector<SetupLock>{SetupLock{dial_ref(), "step"}});
}
