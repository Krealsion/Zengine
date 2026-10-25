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
#include <optional>
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
    std::vector<v2::PaneSettings> handed;
    std::vector<bool> legend_drawn; ///< per picture
    std::optional<gfx::AreaRoom> last_room;
    /// The size of the area the Terminal held when the tap began, when a case says it: a first
    /// room of that size is the one it had, not a new one.
    std::optional<std::pair<std::int64_t, std::int64_t>> held;
    explicit TerminalTap(loom::Switchboard& on) : bus(on) {
        tap = bus.add_observer([this](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered || ev.payload == nullptr) {
                return;
            }
            const loom::WeaveId terminal = bus.role_holder(tp::kTerminalPaneRole);
            if (ev.target == terminal && ev.schema_name == v2::PaneSettings::zen_name) {
                // THE TERMINAL TAKES v2 ALONE, so no other version may cross to it, and from the
                // layouts' owner alone.
                CHECK(ev.schema_version == v2::PaneSettings::zen_version);
                CHECK(ev.authored_role == kLayoutsRole);
                heard.push_back("settings");
                handed.push_back(loom::from_value<v2::PaneSettings>(*ev.payload));
            } else if (ev.target == terminal && ev.schema_name == gfx::AreaRoom::zen_name) {
                // A NEW ROOM: the window says the room it gives the area again whenever it is told
                // of the area again, and only one that moved, or an area emptied, is new.
                const auto r = loom::from_value<gfx::AreaRoom>(*ev.payload);
                const bool fresh =
                    !last_room ? (!held || r.empty || r.w != held->first || r.h != held->second)
                               : (r.empty || r.window != last_room->window ||
                                  r.epoch != last_room->epoch || r.w != last_room->w ||
                                  r.h != last_room->h || r.measure() != last_room->measure());
                last_room = r;
                if (fresh) heard.push_back("room");
            } else if (ev.authored_role == tp::kTerminalPaneRole &&
                       ev.schema_name == gfx::Drawn::zen_name) {
                // WHAT THE AREA SHOWS ONCE THIS DRAWING IS TAKEN: the rig's mirror, which watches
                // the bus first, holds it already.
                bool legend = false;
                const zengine::graphics::window::WinArea* area =
                    held_desks().empty() ? nullptr
                                         : held_desks().back().mirror->area(tp::kTerminalPaneRole);
                for (const zengine::graphics::window::KeptNode& k :
                     area == nullptr ? std::vector<zengine::graphics::window::KeptNode>{}
                                     : area->nodes) {
                    if (const auto t = gfx::from_body<gfx::TextArgs>(k.node.args)) {
                        legend = legend || t->text.find(kLegendText) != std::string::npos;
                    }
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
    /// Whether a hand-off crossed after `from` before the Terminal's first picture since did, and a
    /// picture followed: the pane draws no room before it holds a row.
    bool settings_before_picture_since(std::size_t from) const {
        for (std::size_t i = from; i < heard.size(); ++i) {
            if (heard[i] == "picture") {
                return false;
            }
            if (heard[i] == "settings") {
                return count_since(i, "picture") > 0;
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
    void unfocus() { r.press_cell(0, gfx::cell_of_pixel(screen_of(r.session()).h) - 1); }

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
    /// NAME THE TERMINAL AS INFO'S SUBJECT, as the Pane Manager's `inspect in Info` row does.
    void inspect_terminal() {
        REQUIRE(hand_inspect_in_info(r, terminal_ref()).accepted);
        REQUIRE(r.session().inspected.ref == terminal_ref());
    }
    /// A DRAFT ON A SETTINGS ROW, its text replaced by `typed`.
    void draft(const std::string& key, const std::string& typed) {
        const CellRect body = external_body_rect(r.session(), info());
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
    CHECK(terminal_settings(t.r.run().active) == std::vector<PaneSetting>{legend_off()});
    CHECK_FALSE(t.legend_shown());
    // ...INFO SAYS HOW FAR THE HAND-OFF GOT ON ITS `Settings` ROW, AS THE LAYOUTS' OWNER READS IT,
    // AND WORKSHOP'S COPY OF THE ROW HOLDS NONE OF THE PANE'S OWN SETTINGS.
    // ⚔ MUTATION: the owner's picture carrying the pane's own keys -- Workshop holds the legend.
    bool fact = false;
    for (const std::string& row : t.info_rows()) {
        fact = fact || (row.find("Settings") != std::string::npos &&
                        row.find("stored and handed") != std::string::npos);
    }
    CHECK(fact);
    CHECK(terminal_settings(t.r.session().setup.active).empty());

    // EACH SWITCH HANDS THE TERMINAL ITS LAYOUT'S CHOICE ONCE, AND GRANTS IT NO NEW ROOM.
    TerminalTap tap(t.r.bus);
    {
        const RuntimePane* row = t.r.session().panes.runtime.find(tp::kTerminalPaneRole, tp::kTerminalPane);
        REQUIRE(row != nullptr);
        const ExternalPane* seated = t.r.session().panes.external_pane(row->kind);
        REQUIRE(seated != nullptr);
        tap.held = std::pair{seated->placed_w, seated->placed_h};
    }
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
    for (const v2::PaneSettings& one : tap.handed) {
        CHECK(one.pane == tp::kTerminalPane);
    }
}

TEST_CASE("a Terminal seated again holds its layout's legend before its first picture, and that picture keeps it") {
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

    // FROM C BACK TO A: the Terminal is seated again, and holds its row before it draws.
    TerminalTap tap(t.r.bus);
    while (t.r.session().setup.active_at != a) {
        layout_key(t.r, Act::kLayoutNext);
    }
    REQUIRE(t.seated());
    CHECK(tap.settings_before_picture_since(0));
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
    REQUIRE(terminal_settings(back.r.run().active) == std::vector<PaneSetting>{legend_off()});
    back.load_images();
    back.r.ready(); // ...and the catalog asked again, which every image answers by re-offering
    REQUIRE(back.seated());
    CHECK(tap.settings_before_picture_since(0));
    REQUIRE_FALSE(tap.legend_drawn.empty());
    for (std::size_t i = 0; i < tap.legend_drawn.size(); ++i) {
        CAPTURE(i);
        CHECK_FALSE(tap.legend_drawn[i]);
    }
    CHECK_FALSE(back.legend_shown());

    // A RELOAD OF THE SAME IMAGE HOLDS NOTHING IT WAS HANDED: IT ASKS, AND HOLDS ITS ROW BEFORE
    // ITS FIRST PICTURE.
    // ⚔ MUTATION: a pane that draws its room before it holds a row -- the reloaded image's first
    // picture shows the legend its layout hides.
    const std::size_t mark = tap.heard.size();
    const std::string image =
        dir.file(("terminal-again" +
                  std::filesystem::path(WORKSHOP_SO_TERMINAL_PANE).extension().string())
                     .c_str());
    std::filesystem::copy_file(WORKSHOP_SO_TERMINAL_PANE, image);
    back.r.enqueue_reload(tp::kTerminalPaneStem, image);
    back.r.bus.drain_until_idle();
    REQUIRE(back.r.load_refusals.empty());
    CHECK(tap.settings_before_picture_since(mark));
    for (std::size_t i = 0; i < tap.legend_drawn.size(); ++i) {
        CAPTURE(i);
        CHECK_FALSE(tap.legend_drawn[i]);
    }
    CHECK_FALSE(back.legend_shown());
}

TEST_CASE("an Info commit queued behind a layout switch is refused, and neither layout is written") {
    // THE RACE, THROUGH ORDINARY INPUT: Return, a press on the band and the layout key in one poll.
    // Workshop resolves the Return to Info's commit and puts the other desk live before Info has
    // sent it, so the commit names a revision of a row the live layout no longer stands at.
    LegendRig t;
    t.open();
    t.unfocus();
    duplicate_live_layout(t.r);
    t.inspect_terminal();
    t.draft(tp::kSettingLegend, "off");
    // WHAT INFO WAS ANSWERED, read off the bus: the commit it sent the layouts' owner, refused.
    std::vector<PaneSettingCommitAnswered> answered;
    const loom::ObserverId tap = t.r.bus.add_observer([&t, &answered](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.payload != nullptr &&
            ev.target == t.r.bus.role_holder(ip::kInfoPaneRole) &&
            ev.schema_name == PaneSettingCommitAnswered::zen_name) {
            answered.push_back(loom::from_value<PaneSettingCommitAnswered>(*ev.payload));
        }
    });
    const Gesture next = t.r.session().keymap.gesture_of(Act::kLayoutNext);
    // ONE POLL OF THE WINDOW'S PLATFORM: the three moments read together on its next beat.
    t.r.windows.queue({input::KeyPressed{input::scan::kReturn, "", input::mod::kNone},
                       input::PointerButton{1, true, 0,
                                            gfx::cell_of_pixel(screen_of(t.r.session()).h) - 1,
                                            input::space::kCells, input::mod::kNone},
                       input::KeyPressed{next.scancode, "", next.modifiers}});
    t.r.windows.pump();
    t.r.bus.remove_observer(tap);
    REQUIRE(answered.size() == 1);
    CHECK_FALSE(answered.front().accepted);
    CHECK(answered.front().refusal.rfind("stale revision ", 0) == 0);
    CHECK(t.r.last_notice().find("committed") == std::string::npos);
    for (const Layout& layout : t.r.run().shelved) {
        CHECK(terminal_settings(layout.desk).empty());
    }
    CHECK(terminal_settings(t.r.run().active).empty());
    CHECK(t.legend_shown());
}

// The press lands on the rows drawn with the legend, and says what the window drew there; the
// Terminal reads it against the rows drawn since, where another row stands.
TEST_CASE("a press aimed at the Terminal's picture from before its legend hid is refused") {
    LegendRig t;
    t.open();
    t.inspect_terminal();
    t.unfocus();
    REQUIRE(t.legend_shown());

    // THE COMMIT'S HAND-OFF IS QUEUED BY THE LAYOUTS' OWNER, AND ONLY THEN DOES A PRESS ARRIVE:
    // Workshop stamps it with the picture it holds, drawn with the legend, and the Terminal is
    // handed the setting first.
    PaneSettingCommitRequested off;
    off.provider = terminal_ref().provider;
    off.pane = terminal_ref().pane;
    off.revision = info_read(t.r, terminal_ref()).revision;
    off.key = tp::kSettingLegend;
    off.flag = false;
    DoorHand& h = door_hand(t.r);
    h.next = [off](DoorHand&, loom::Mail& m) {
        (void)m.as_role(DoorHand::kOffice).send_to_role(kLayoutsRole, off);
    };
    (void)t.r.bus.send(t.r.hand_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
    (void)t.r.bus.pump_pending(); // the hand sends the commit
    (void)t.r.bus.pump_pending(); // the owner writes it, queues the hand-off and the live layout
    const CellRect body = external_body_rect(t.r.session(), t.terminal());
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
    REQUIRE(terminal_settings(t.r.run().active) == pane_of(desk, terminal_ref())->settings);

    // THE PANE KEEPS ITS DEFAULT AND SAYS WHY WHERE THE SETTING SHOWS, ON A ROW NO KEY CLEARS.
    const std::string said = "legend takes on or off, not a text `hiden` -- on";
    CHECK(t.legend_row() == said);
    const CellRect body = external_body_rect(t.r.session(), t.terminal());
    t.r.press_cell(body.x, body.y + body.h - 1); // into the Terminal: its keys go to it
    t.r.text("x");
    CHECK(t.legend_row() == said);

    // ...INFO SHOWS IT AS STORED, AND WHAT IS IN EFFECT; AND `s` WRITES THE ROW AS IT CAME.
    t.inspect_terminal();
    CHECK(info_value(t.r, terminal_ref(), tp::kSettingLegend) == "hiden -- takes on or off; on in effect");
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
    stage_desk(t.r, [&](Setup& desk) {
        REQUIRE(add_pane(desk, terminal_ref()));
    });
    REQUIRE(hand_inspect(t.r, terminal_ref()).accepted);
    CHECK(!info_has(t.r, terminal_ref(), tp::kSettingLegend));
    t.load_images();
    REQUIRE(t.seated());
    t.tall_info();
    // NO SWITCH, NO GESTURE: the declaration alone brought the row.
    REQUIRE(info_has(t.r, terminal_ref(), tp::kSettingLegend));
    CHECK(info_value(t.r, terminal_ref(), tp::kSettingLegend) == "on");

    // A DRAFT ON IT OUTLIVES THE SAME DECLARATION AGAIN, as every catalog request brings one.
    t.draft(tp::kSettingLegend, "off");
    const std::int64_t name = t.r.session().inspected.name;
    t.r.ready();
    CHECK(t.r.session().inspected.name == name);
    t.r.key(input::scan::kReturn);
    CHECK(terminal_settings(t.r.run().active) == std::vector<PaneSetting>{legend_off()});
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
    CHECK(terminal_settings(t.r.run().active).empty());
    CHECK(t.legend_shown());
}
