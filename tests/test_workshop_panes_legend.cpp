// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a layout's setting, end to end: the Terminal's legend, declared by
// the real `zengine-terminal-pane` image, written through the real `zengine-info-pane` image's
// rows, and handed back across a switch, a re-seat, a restart and a reload -- each picture the
// Terminal draws already drawn with its layout's choice.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "info/vocabulary.hpp"
#include "terminal/vocabulary.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace {

namespace tp = zengine::terminal_pane;
namespace ip = zengine::info_pane;

constexpr const char* kLegendText = "SUBMITTED = authored; a sender is not told its fate";

PaneRef terminal_ref() { return PaneRef{tp::kTerminalPaneRole, tp::kTerminalPane}; }
PaneSetting legend_off() { return PaneSetting{tp::kSettingLegend, false, {}, {}}; }

/// WHAT CROSSED TO AND FROM THE TERMINAL, in order, read off the bus: each hand-off, each room and
/// each picture, and whether that picture drew the legend. The Terminal is found by its office at
/// each event, so the tap holds before its image loads and across a reload.
struct TerminalTap {
    loom::Switchboard& bus;
    loom::ObserverId tap{};
    std::vector<std::string> heard; ///< "settings", "room" or "picture", in the order they crossed
    std::vector<PaneSettings> handed;
    std::vector<bool> legend_drawn; ///< per picture
    explicit TerminalTap(loom::Switchboard& on) : bus(on) {
        tap = bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered || ev.payload == nullptr) {
                return;
            }
            const loom::WeaveId terminal = bus.role_holder(tp::kTerminalPaneRole);
            if (ev.target == terminal && ev.schema_name == PaneSettings::zen_name) {
                heard.push_back("settings");
                handed.push_back(loom::from_value<PaneSettings>(*ev.payload));
            } else if (ev.target == terminal && (ev.schema_name == PaneCanvasRoom::zen_name ||
                                                 ev.schema_name == PaneRoom::zen_name)) {
                heard.push_back("room");
            } else if (ev.authored_role == tp::kTerminalPaneRole &&
                       ev.schema_name == v5::PaneCanvasContent::zen_name &&
                       ev.schema_version == v5::PaneCanvasContent::zen_version) {
                const v5::PaneCanvasContent picture =
                    loom::from_value<v5::PaneCanvasContent>(*ev.payload);
                bool legend = false;
                for (const v2::PaneCanvasText& t : picture.texts) {
                    legend = legend || t.text.find(kLegendText) != std::string::npos;
                }
                heard.push_back("picture");
                legend_drawn.push_back(legend);
            }
        });
    }
    ~TerminalTap() { bus.remove_observer(tap); }
    TerminalTap(const TerminalTap&) = delete;
    TerminalTap& operator=(const TerminalTap&) = delete;

    /// How many of `what` crossed after the first `from` entries.
    std::size_t count_since(std::size_t from, const char* what) const {
        std::size_t n = 0;
        for (std::size_t i = from; i < heard.size(); ++i) {
            n += heard[i] == what ? 1u : 0u;
        }
        return n;
    }
    /// Whether a hand-off crossed after `from` before any room did.
    bool settings_before_room_since(std::size_t from) const {
        for (std::size_t i = from; i < heard.size(); ++i) {
            if (heard[i] == "room") {
                return false;
            }
            if (heard[i] == "settings") {
                return true;
            }
        }
        return false;
    }
};

/// A LIVE WORKSHOP WITH THE REAL INFO AND TERMINAL IMAGES: Info on the shipped desk, the Terminal
/// picked onto it, and Info driven by presses and keys as a weaver drives it.
struct LegendRig {
    PaneRig r;
    loom::TerminalSession* me = nullptr;

    void load_images() {
        load::LoadPlan plan;
        for (const auto& [stem, role] :
             {std::pair<const char*, const char*>{ip::kInfoPaneStem, ip::kInfoPaneRole},
              std::pair<const char*, const char*>{tp::kTerminalPaneStem, tp::kTerminalPaneRole}}) {
            load::ArtifactIntent seat;
            seat.stem = stem;
            seat.weave = load::WeaveIntent{role};
            plan.artifacts.push_back(seat);
        }
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.bus.drain_until_idle();
    }
    void open() {
        r.mount_workshop();
        me = r.mount_terminal(0, false);
        load_images();
        r.ready();
        r.extent(160, 48);
        r.pick(terminal_ref());
        REQUIRE(seated());
        tall_info();
    }
    /// INFO TALL ENOUGH TO SHOW A SUBJECT'S EVERY ROW, its list and SETTINGS included.
    void tall_info() {
        author_test_pane_room(r, info(), 40, 70);
        r.key(input::scan::kUnknown);
    }
    std::int64_t terminal() {
        const RuntimePane* row = r.session().panes.runtime.find(tp::kTerminalPaneRole, tp::kTerminalPane);
        REQUIRE(row != nullptr);
        return row->kind;
    }
    std::int64_t info() {
        const RuntimePane* row = r.session().panes.runtime.find(ip::kInfoPaneRole, ip::kInfoPane);
        REQUIRE(row != nullptr);
        return row->kind;
    }
    bool seated() {
        const RuntimePane* row = r.session().panes.runtime.find(tp::kTerminalPaneRole, tp::kTerminalPane);
        return row != nullptr && r.session().panes.has(row->kind);
    }
    /// WHETHER THE TERMINAL'S PICTURE, as Workshop admitted it, holds the legend: its text, and the
    /// part the Terminal names it by.
    bool legend_shown() {
        const ExternalPane* seat = r.session().panes.external_pane(terminal());
        REQUIRE(seat != nullptr);
        bool text = false;
        for (const std::string& row : held_row_texts(*seat)) {
            text = text || row.find(kLegendText) != std::string::npos;
        }
        const bool part = held_parts(r.session(), terminal()).count(tp::kSettingLegend) == 1;
        CHECK(text == part);
        return text;
    }
    std::string legend_row() {
        const ExternalPane* seat = r.session().panes.external_pane(terminal());
        REQUIRE(seat != nullptr);
        const auto parts = held_parts(r.session(), terminal());
        const auto at = parts.find(tp::kSettingLegend);
        if (at == parts.end()) {
            return std::string();
        }
        return held_row_texts(*seat).at(static_cast<std::size_t>(at->second.row));
    }
    /// A press on the bottom band: the keys go back to Workshop, which answers the layout keys.
    void unfocus() { r.press_cell(0, screen_of(r.session()).h - 1); }

    // ---- Info, as a weaver drives it ----------------------------------------------------------
    std::vector<std::string> info_rows() { return pane_rows(r, info()); }
    std::int64_t info_row(const std::function<bool(const std::string&)>& is, bool after_settings) {
        const std::vector<std::string> rows = info_rows();
        bool past = !after_settings;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (rows[i].rfind(" SETTINGS", 0) == 0) {
                past = true;
                continue;
            }
            if (past && is(rows[i])) {
                return static_cast<std::int64_t>(i);
            }
        }
        return -1;
    }
    void press_info(std::int64_t row) {
        std::string all;
        for (const std::string& one : info_rows()) {
            all += one + "\n";
        }
        REQUIRE_MESSAGE(row >= 0, "no such row in Info:\n", all);
        press_pane(r, info(), row, 3);
    }
    /// NAME THE TERMINAL AS INFO'S SUBJECT: into Info, a press on its row of the list.
    void inspect_terminal() {
        const ui::Rect body = external_body_rect(r.session(), info());
        r.press_cell(body.x, body.y);
        press_info(info_row([](const std::string& row) { return row.compare(2, 12, "Terminal -- ") == 0; },
                            false));
        REQUIRE(r.session().inspected.ref == terminal_ref());
    }
    /// A DRAFT ON A SETTINGS ROW, its text replaced by `typed`.
    void draft(const std::string& key, const std::string& typed) {
        const ui::Rect body = external_body_rect(r.session(), info());
        r.press_cell(body.x, body.y);
        press_info(info_row([&key](const std::string& row) { return row.compare(1, key.size() + 1, key + " ") == 0; },
                            true));
        r.key(input::scan::kReturn);
        for (int i = 0; i < 64; ++i) {
            r.key(input::scan::kBackspace);
        }
        r.text(typed);
    }
    /// ...AND COMMIT IT, as Return does.
    void write(const std::string& key, const std::string& typed) {
        draft(key, typed);
        r.key(input::scan::kReturn);
    }
};

std::vector<PaneSetting> terminal_settings(const Setup& desk) {
    const SetupPane* row = pane_of(desk, terminal_ref());
    REQUIRE(row != nullptr);
    return row->settings;
}

} // namespace

TEST_CASE("a layout keeps the Terminal's legend hidden while another shows it, written through Info's rows") {
    LegendRig t;
    t.open();
    REQUIRE(t.legend_shown());

    // TWO LAYOUTS, THE TERMINAL AT ONE GEOMETRY IN BOTH: the copy is the one live now.
    t.unfocus();
    duplicate_live_layout(t.r);
    const std::size_t hidden_at = t.r.session().setup.active_at;
    t.inspect_terminal();
    t.write(tp::kSettingLegend, "off");
    CHECK(t.r.last_notice() == "committed legend of Terminal = off; stored and handed");
    CHECK(terminal_settings(t.r.session().setup.active) == std::vector<PaneSetting>{legend_off()});
    CHECK_FALSE(t.legend_shown());

    // EACH SWITCH HANDS THE TERMINAL ITS LAYOUT'S CHOICE ONCE, AND GRANTS IT NO NEW ROOM.
    TerminalTap tap(t.r.bus);
    t.unfocus();
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.r.session().setup.active_at != hidden_at);
    CHECK(t.legend_shown());
    CHECK(tap.count_since(0, "settings") == 1);
    CHECK(tap.count_since(0, "room") == 0);
    CHECK(tap.handed.back().settings.empty());
    const std::size_t mark = tap.heard.size();
    layout_key(t.r, Act::kLayoutNext);
    REQUIRE(t.r.session().setup.active_at == hidden_at);
    CHECK_FALSE(t.legend_shown());
    CHECK(tap.count_since(mark, "settings") == 1);
    CHECK(tap.count_since(mark, "room") == 0);
    CHECK(tap.handed.back().settings == std::vector<PaneSetting>{legend_off()});
    // ...AND WHAT CROSSED NAMES THE SETTING, NEVER A LAYOUT.
    for (const PaneSettings& one : tap.handed) {
        CHECK(one.pane == tp::kTerminalPane);
    }
}

TEST_CASE("a Terminal seated again is handed its layout's legend before its room, and its first picture keeps it") {
    LegendRig t;
    t.open();
    // A KEEPS THE LEGEND OFF; B, A COPY PUT BACK TO THE DEFAULT; C HAS NO TERMINAL.
    t.inspect_terminal();
    t.write(tp::kSettingLegend, "off");
    const std::size_t a = t.r.session().setup.active_at;
    t.unfocus();
    duplicate_live_layout(t.r);
    t.inspect_terminal();
    t.write(tp::kSettingLegend, "-");
    REQUIRE(t.legend_shown());
    t.unfocus();
    layout_key(t.r, Act::kLayoutNew);
    REQUIRE_FALSE(has_pane(t.r.session().setup.active, terminal_ref()));
    REQUIRE_FALSE(t.seated());

    // FROM C BACK TO A: the Terminal is seated again, and the hand-off precedes its room.
    TerminalTap tap(t.r.bus);
    while (t.r.session().setup.active_at != a) {
        layout_key(t.r, Act::kLayoutNext);
    }
    REQUIRE(t.seated());
    CHECK(tap.settings_before_room_since(0));
    REQUIRE_FALSE(tap.legend_drawn.empty());
    CHECK_FALSE(tap.legend_drawn.front());
    CHECK_FALSE(t.legend_shown());
}

TEST_CASE("after a restart the Terminal's first picture keeps its layout's legend, and so does a reload's") {
    TempDir dir("legend-restart");
    const std::string session = dir.file("session.json");
    {
        LegendRig t;
        t.r.host.session_path = session;
        t.open();
        t.inspect_terminal();
        t.write(tp::kSettingLegend, "off");
        REQUIRE_FALSE(t.legend_shown());
        t.unfocus();
        t.r.key(input::scan::kQ);
        REQUIRE(t.r.host.quit);
    }
    REQUIRE(std::filesystem::exists(session));

    // THE SHIPPED ORDER: the surface says hello and the last session is restored before any image
    // offers; the Terminal then offers, and declares only after its offer.
    LegendRig back;
    back.r.host.session_path = session;
    back.r.mount_workshop();
    back.me = back.r.mount_terminal(0, false);
    TerminalTap tap(back.r.bus);
    back.r.ready();
    back.r.extent(160, 48);
    REQUIRE(terminal_settings(back.r.session().setup.active) == std::vector<PaneSetting>{legend_off()});
    back.load_images();
    back.r.ready(); // ...and the catalog asked again, which every image answers by re-offering
    REQUIRE(back.seated());
    CHECK(tap.settings_before_room_since(0));
    REQUIRE_FALSE(tap.legend_drawn.empty());
    for (std::size_t i = 0; i < tap.legend_drawn.size(); ++i) {
        CAPTURE(i);
        CHECK_FALSE(tap.legend_drawn[i]);
    }
    CHECK_FALSE(back.legend_shown());

    // A RELOAD OF THE SAME IMAGE HOLDS NOTHING IT WAS HANDED, AND IS HANDED IT AGAIN FIRST.
    const std::size_t mark = tap.heard.size();
    const std::string image =
        dir.file(("terminal-again" +
                  std::filesystem::path(WORKSHOP_SO_TERMINAL_PANE).extension().string())
                     .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_TERMINAL_PANE, image);
    back.r.enqueue_reload(tp::kTerminalPaneStem, image);
    back.r.bus.drain_until_idle();
    REQUIRE(back.r.load_refusals.empty());
    CHECK(tap.settings_before_room_since(mark));
    for (std::size_t i = 0; i < tap.legend_drawn.size(); ++i) {
        CAPTURE(i);
        CHECK_FALSE(tap.legend_drawn[i]);
    }
    CHECK_FALSE(back.legend_shown());
}

TEST_CASE("an Info commit queued behind a layout switch is refused, and neither layout is written") {
    // THE RACE, THROUGH ORDINARY INPUT: Return, a press on the band and the layout key in one poll.
    // Workshop resolves the Return to Info's commit and puts the other desk live before Info has
    // sent it, so the commit names rows the host no longer holds.
    LegendRig t;
    t.open();
    t.unfocus();
    duplicate_live_layout(t.r);
    t.inspect_terminal();
    t.draft(tp::kSettingLegend, "off");
    const auto enqueue = [&t](const loom::Value& v) {
        (void)t.r.bus.publish(loom::Message(v, loom::WeaveId{}, loom::WeaveId{}, 0));
    };
    // WHAT INFO WAS ANSWERED, read off the bus: the commit it sent, and the refusal.
    std::vector<PaneSubjectActed> answered;
    const loom::ObserverId tap = t.r.bus.add_observer([&t, &answered](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.payload != nullptr &&
            ev.target == t.r.bus.role_holder(ip::kInfoPaneRole) &&
            ev.schema_name == PaneSubjectActed::zen_name) {
            answered.push_back(loom::from_value<PaneSubjectActed>(*ev.payload));
        }
    });
    const Gesture next = t.r.session().keymap.gesture_of(Act::kLayoutNext);
    enqueue(loom::to_value(input::KeyPressed{input::scan::kReturn, "", input::mod::kNone}));
    enqueue(loom::to_value(input::PointerButton{1, true, 0,
                                                screen_of(t.r.session()).h - 1 + surface::kTuiCanvasTopRow,
                                                input::space::kCells, input::mod::kNone}));
    enqueue(loom::to_value(input::KeyPressed{next.scancode, "", next.modifiers}));
    t.r.bus.drain_until_idle();
    t.r.bus.remove_observer(tap);
    REQUIRE(answered.size() == 1);
    CHECK_FALSE(answered.front().accepted);
    CHECK(answered.front().refusal == WorkshopWeave::kPaneCommitSubjectGone);
    CHECK(t.r.last_notice().find("committed") == std::string::npos);
    for (const Layout& layout : t.r.session().setup.shelved) {
        CHECK(terminal_settings(layout.desk).empty());
    }
    CHECK(terminal_settings(t.r.session().setup.active).empty());
    CHECK(t.legend_shown());
}

TEST_CASE("a press aimed at the Terminal's picture from before its legend hid is refused") {
    LegendRig t;
    t.open();
    t.inspect_terminal();
    t.unfocus();
    REQUIRE(t.legend_shown());

    // THE COMMIT'S HAND-OFF IS QUEUED, AND ONLY THEN DOES A PRESS ARRIVE: Workshop stamps it with
    // the picture it holds, drawn with the legend, and the Terminal is handed the setting first.
    const std::size_t at = subject_row_index(t.r.session(), tp::kSettingLegend, "SETTINGS");
    const std::int64_t subject = t.r.session().inspected.name;
    DoorHand& h = door_hand(t.r);
    h.next = [subject, at](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice)
            .send_to_role(kWorkshopProvider,
                          PaneCommitRequested{subject, static_cast<std::int64_t>(at), "off"});
    };
    (void)t.r.bus.send(t.r.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    (void)t.r.bus.pump_pending(); // the hand sends the commit
    (void)t.r.bus.pump_pending(); // Workshop writes it and queues the hand-off
    const ui::Rect body = external_body_rect(t.r.session(), t.terminal());
    t.r.press_cell(body.x + 3, body.y + 2); // a row the legend's going moves
    REQUIRE_FALSE(t.legend_shown());
    bool refused = false;
    for (const std::string& row : held_row_texts(*t.r.session().panes.external_pane(t.terminal()))) {
        refused = refused || row.find("That transcript picture changed; try again") != std::string::npos;
    }
    CHECK(refused);
}

TEST_CASE("a legend value the Terminal cannot use is kept as stored, and said in its legend row") {
    TempDir dir("legend-unusable");
    LegendRig t;
    t.r.host.setup_path = dir.file("setup.json");
    t.open();
    // A DESK WRITTEN BY HAND, AS AN AGENT'S DESK IS: its legend a text, which the setting does not take.
    Setup desk = t.r.session().setup.active;
    pane_of(desk, terminal_ref())->settings = {PaneSetting{tp::kSettingLegend, {}, {}, std::string("hiden")}};
    const std::string bytes = setup_persist::to_text(desk);
    spillout(t.r.host.setup_path, bytes);
    t.unfocus();
    t.r.key(input::scan::kR);
    REQUIRE(terminal_settings(t.r.session().setup.active) == pane_of(desk, terminal_ref())->settings);

    // THE PANE KEEPS ITS DEFAULT AND SAYS WHY WHERE THE SETTING SHOWS, ON A ROW NO KEY CLEARS.
    const std::string said = "legend takes on or off, not a text `hiden` -- on";
    CHECK(t.legend_row() == said);
    const ui::Rect body = external_body_rect(t.r.session(), t.terminal());
    t.r.press_cell(body.x, body.y + body.h - 1); // into the Terminal: its keys go to it
    t.r.text("x");
    CHECK(t.legend_row() == said);

    // ...INFO SHOWS IT AS STORED, AND WHAT IS IN EFFECT; AND `s` WRITES THE ROW AS IT CAME.
    t.inspect_terminal();
    CHECK(subject_value(t.r.session(), tp::kSettingLegend, "SETTINGS") == "hiden -- takes on or off; on in effect");
    t.unfocus();
    t.r.key(input::scan::kS);
    const setup_persist::LoadedSetup saved = setup_persist::load_file(t.r.host.setup_path);
    REQUIRE(saved.outcome.accepted);
    CHECK(terminal_settings(saved.setup) == pane_of(desk, terminal_ref())->settings);
}

TEST_CASE("the Terminal's settings rows appear the moment its image declares, and an identical declaration keeps a draft") {
    LegendRig t;
    t.r.mount_workshop();
    t.me = t.r.mount_terminal(0, false);
    t.r.ready();
    t.r.extent(160, 48);
    // THE DESK NAMES THE TERMINAL BEFORE ANY IMAGE OFFERS IT, and the inspector names it too.
    Session& s = const_cast<Session&>(t.r.session());
    REQUIRE(add_pane(s.setup.active, terminal_ref()));
    REQUIRE(hand_inspect(t.r, terminal_ref()).accepted);
    CHECK(subject_row(t.r.session(), tp::kSettingLegend, "SETTINGS") == nullptr);
    t.load_images();
    REQUIRE(t.seated());
    t.tall_info();
    // NO SWITCH, NO GESTURE: the declaration alone brought the row.
    REQUIRE(subject_row(t.r.session(), tp::kSettingLegend, "SETTINGS") != nullptr);
    CHECK(subject_value(t.r.session(), tp::kSettingLegend, "SETTINGS") == "on");

    // A DRAFT ON IT OUTLIVES THE SAME DECLARATION AGAIN, as every catalog request brings one.
    t.draft(tp::kSettingLegend, "off");
    const std::int64_t name = t.r.session().inspected.name;
    t.r.ready();
    CHECK(t.r.session().inspected.name == name);
    t.r.key(input::scan::kReturn);
    CHECK(terminal_settings(t.r.session().setup.active) == std::vector<PaneSetting>{legend_off()});
    CHECK_FALSE(t.legend_shown());
}

TEST_CASE("a Terminal closed in a layout that hid its legend shows it again when it is shown") {
    LegendRig t;
    t.open();
    t.inspect_terminal();
    t.write(tp::kSettingLegend, "off");
    REQUIRE_FALSE(t.legend_shown());
    // THE CLOSE TAKES THE ROW AND ITS SETTING; THE ROW SHOWN AGAIN KEEPS NONE, AND THE TERMINAL --
    // which never left, and still holds what it was last handed -- is handed that too.
    REQUIRE(hand_close(t.r, terminal_ref()).closed);
    CHECK(t.r.last_notice().find("hid Terminal and its settings here (legend off) -- ") == 0);
    REQUIRE(seat_pane(t.r, terminal_ref()).opened);
    CHECK(terminal_settings(t.r.session().setup.active).empty());
    CHECK(t.legend_shown());
}
