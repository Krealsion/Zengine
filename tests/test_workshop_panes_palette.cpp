// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the Command Palette (WL-DESK-18): what it lists and how a filter
// orders it, read from Workshop's own sayings; the real palette image opened by the desktop's key
// over a pane, each kind of entry run by Workshop's door as the hand that chose it, and put down
// with the keys given back; and the door refusing a run under any number but the palette's own row
// and from any office but the palette's. One source of the panes program, run in
// `workshop_panes_hand`.

#include "doctest.h"

#include "workshop_support.hpp"
#include "census.hpp"

#include "workshop/command_vocabulary.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"
#include "workshop/palette/entries.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace {

namespace dp = zengine::desktop_pane;
namespace pal = zengine::palette;

/// A STAND-IN IN AN OFFICE OF A CASE'S CHOOSING -- the palette's, or a stranger's -- that records
/// the numbers its rows came under and Workshop's answers, and asks a command run under whatever
/// number a case gives it.
class CommandAsker
    : public loom::WeaveBase<CommandAsker, SeatState,
                             loom::Accept<PaneCatalogRequested, PaneRoom, PaneActionRequested,
                                          CommandRunAnswered, SeatDo>,
                             loom::Emit<v2::PaneOffered, v3::PaneActions, CommandRunRequested>> {
public:
    explicit CommandAsker(std::string office) : office_(std::move(office)) {}
    std::vector<std::uint64_t> asks;
    std::vector<CommandRunAnswered> answers;
    std::function<void(CommandAsker&, loom::Mail&)> next;

    void on(const PaneCatalogRequested&, loom::Mail&) {}
    void on(const PaneRoom&, loom::Mail&) {}
    void on(const PaneActionRequested&, loom::Mail& mail) { asks.push_back(mail.correlation()); }
    void on(const CommandRunAnswered& a, loom::Mail&) { answers.push_back(a); }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(CommandAsker&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
    void say(loom::Mail& mail, const CommandRunRequested& asked, std::uint64_t correlation) {
        (void)mail.as_role(office_).send_to_role(kWorkshopProvider, asked, correlation);
    }
    const std::string& office() const { return office_; }

private:
    std::string office_;
};

struct Asker {
    CommandAsker* seat = nullptr;
    loom::WeaveId id{};
};

Asker mount_asker(PaneRig& r, const std::string& office) {
    auto weave = std::make_unique<CommandAsker>(office);
    CommandAsker* raw = weave.get();
    loom::Grant grant;
    grant.allow_to_any(v2::PaneOffered::zen_name, v2::PaneOffered::zen_version);
    grant.allow_to_any(v3::PaneActions::zen_name, v3::PaneActions::zen_version);
    grant.allow_to_any(CommandRunRequested::zen_name, CommandRunRequested::zen_version);
    const loom::WeaveId id = r.bus.register_weave(std::move(weave), std::move(grant), office);
    raw->zen_set_self(id);
    return Asker{raw, id};
}

void drive(PaneRig& r, const Asker& a, std::function<void(CommandAsker&, loom::Mail&)> what) {
    a.seat->next = std::move(what);
    (void)r.bus.send(a.id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                         loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
}

std::int64_t kind_of(PaneRig& r, const char* office, const char* pane) {
    const RuntimePane* row = r.session().panes.runtime.find(office, pane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

/// A PRESS INTO A PANE'S BODY, which gives it the keys.
void focus(PaneRig& r, const char* office, const char* pane) {
    r.pick(PaneRef{office, pane});
    const ui::Rect body = external_body_rect(r.session(), kind_of(r, office, pane));
    r.press_cell(body.x + 1, body.y);
}

bool on_desk(PaneRig& r, const char* office, const char* pane) {
    return has_pane(r.session().setup.active, PaneRef{office, pane});
}

constexpr const char* kTools = "zengine.test.tools";

/// THE DESK A PALETTE CASE STARTS FROM: Workshop, two fixture panes -- `alpha` declaring a row with
/// no key -- the real desktop, and `alpha` holding the keys.
void start(PaneRig& r, ProviderSeat*& tools) {
    r.mount_workshop();
    r.ready();
    r.extent(160, 60);
    tools = r.mount_provider(kTools);
    r.drive(tools, [](ProviderSeat& s, loom::Mail& m) {
        s.offer(m, PaneOffered{"alpha", "Alpha", "a fixture"});
        s.offer(m, PaneOffered{"beta", "Beta", "a fixture"});
        v3::PaneActions rows;
        rows.pane = "alpha";
        rows.rows.push_back(v3::PaneActionRow{"alpha.poke", "poke the fixture",
                                              input::scan::kUnknown, input::mod::kNone, "", {}});
        (void)m.as_role(kTools).send_to_role(kWorkshopProvider, rows);
    });
    const loom::WeaveId desktop = r.load(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE, kDesktopRole);
    REQUIRE(desktop.valid());
    REQUIRE(r.load_refusals.empty());
    focus(r, kTools, "alpha");
    REQUIRE(r.session().panes.keyboard == kind_of(r, kTools, "alpha"));
}

/// THE PALETTE AS IT DECLARES ITS ROWS, on a desk of its own: Workshop's record of it.
const RuntimePane& palette_held(PaneRig& r, ProviderSeat*& tools) {
    start(r, tools);
    REQUIRE(r.load("zengine-palette", WORKSHOP_SO_PALETTE, kPaletteRole).valid());
    const RuntimePane* held = r.session().panes.runtime.find(kPaletteRole, kPalettePane);
    REQUIRE(held != nullptr);
    return *held;
}

/// THE PALETTE'S CENSUS: its four rows, in no mode.
const CensusDriverOf kPaletteCensus{kPaletteRole, [] {
    OfficeCensus census;
    PaneRig r;
    ProviderSeat* tools = nullptr;
    census_take(census, kPaletteRole, palette_held(r, tools));
    return census;
}};

/// THE PALETTE'S PRESSES: each row on a palette made afresh.
const PressDriverOf kPalettePresses{kPaletteRole, [](PressWitness& w) {
    std::vector<v3::PaneActionRow> rows;
    {
        PaneRig r;
        ProviderSeat* tools = nullptr;
        rows = palette_held(r, tools).actions;
    }
    for (const v3::PaneActionRow& row : rows) {
        PaneRig r;
        ProviderSeat* tools = nullptr;
        (void)palette_held(r, tools);
        w.judge(r, kPalettePane, "", row);
    }
}};

ShownBinding row(std::string group, std::string office, std::string pane, std::string id,
                           std::string label, std::string gesture, bool authored = false) {
    ShownBinding b;
    b.group = std::move(group);
    b.office = std::move(office);
    b.pane = std::move(pane);
    b.id = std::move(id);
    b.label = std::move(label);
    b.gesture = gesture;
    b.shown = gesture;
    b.authored = authored;
    return b;
}

} // namespace

TEST_CASE("the palette lists every action and pane a hand can reach, filters by each word, and "
          "puts what was chosen first") {
    KeymapShown keys;
    keys.rows.push_back(row("above every mode -- the application's", kDesktopRole, "",
                            "desktop.terminal", "terminal", "ctrl+t"));
    keys.rows.back().causes.push_back({"PaneLaunchRequested", 1, "to zengine.terminal/terminal"});
    keys.rows.push_back(row("above every mode -- the application's", kDesktopRole, "",
                            "desktop.palette", "palette", "ctrl+space"));
    keys.rows.push_back(row("last, where nothing more specific took the key -- the application's",
                            kDesktopRole, "", "desktop.deselect", "put down", "escape"));
    keys.rows.push_back(row("command mode", kWorkshopProvider, "", "workshop.pane-titles", "titles",
                            "t"));
    keys.rows.push_back(row("command mode", kWorkshopProvider, "", "workshop.pane-titles", "titles",
                            "shift+t"));
    keys.rows.push_back(row("the contextual actions", kWorkshopProvider, "", "context.up", "up",
                            "up"));
    keys.rows.push_back(row("pane Flow @zengine.flow", "zengine.flow", "flow", "run", "Run Flow",
                            "ctrl+r"));
    keys.rows.push_back(row("pane Flow @zengine.flow", "zengine.flow", "flow", "apply",
                            "Apply to the running graph", ""));
    keys.rows.push_back(row("pane Flow @zengine.flow", "zengine.flow", "flow", "stop",
                            "Stop the running graph", "", true));
    keys.rows.push_back(row("pane Beta @zengine.test.tools", kTools, "beta", "beta.go", "go", "g"));
    keys.rows.push_back(row(kPaletteRole, kPaletteRole, kPalettePane, "palette.run", "run", "return"));
    keys.rows.push_back(row("the text box's own keys", "", "", "", "select all", "ctrl+a"));
    keys.rows.back().remappable = false;
    PaneInventory inv;
    inv.panes.push_back({"zengine.flow", "flow", "Flow", "graphs", true, true, false});
    inv.panes.push_back({kTools, "beta", "Beta", "a fixture", false, true, false});
    inv.panes.push_back({"zengine.terminal", "terminal", "Terminal", "commands", false, true, false});
    inv.panes.push_back({"zengine.gone", "gone", "Gone", "nothing offers it", false, false, false});
    inv.panes.push_back({kPaletteRole, kPalettePane, "Palette", "this", true, true, false});

    const pal::Listing l = pal::entries_of(keys, inv);
    std::vector<std::string> ids;
    for (const pal::Entry& e : l.entries) ids.push_back(e.kind + ":" + e.id + ":" + e.label);
    CAPTURE(ids);
    // ACTIONS: the application's above every mode, Workshop's command mode once for two keys, the
    // open pane's rows; never the default row, a transient mode's, a closed pane's, the text box's
    // own, the palette's, or the key that opens it.
    REQUIRE(l.entries.size() == 9);
    CHECK(l.entries[0].id == "desktop.terminal");
    CHECK(l.entries[0].owner == "Desktop");
    CHECK(l.entries[1].id == "workshop.pane-titles");
    CHECK(l.entries[1].owner == "Workshop");
    CHECK(l.entries[1].key == "t");
    CHECK(l.entries[1].more_keys == 1);
    CHECK(l.entries[2].id == "run");
    CHECK(l.entries[2].owner == "Flow");
    CHECK(l.entries[3].key == "-");       // a row bound to no key
    CHECK(l.entries[4].key == "off");     // one the weaver switched off
    CHECK(l.entries[4].off);
    // PANES, every one but the palette, the key that opens one beside it, one nothing offers gone.
    CHECK(l.entries[5].kind == kCommandPane);
    CHECK(l.entries[5].label == "Flow");
    CHECK(l.entries[7].label == "Terminal");
    CHECK(l.entries[7].key == "ctrl+t");
    CHECK(l.entries[8].label == "Gone");
    CHECK(l.entries[8].gone);
    CHECK(l.left_out == 0);

    // A FILTER: each word found in the label, the owner or the id, case folded.
    const std::vector<std::size_t> flow_run = pal::shown(l, "FLOW run", {});
    CHECK(flow_run.size() == 3); // `run` is in `running` too
    const std::vector<std::size_t> ctrl_r = pal::shown(l, "flow ctrl+r", {});
    CHECK(ctrl_r.empty());       // a key is not a word an entry is found by
    const std::vector<std::size_t> run_id = pal::shown(l, "Run Flow", {});
    REQUIRE_FALSE(run_id.empty());
    CHECK(l.entries[run_id[0]].id == "run");
    CHECK(pal::shown(l, "running stop", {}).size() == 1);
    CHECK(pal::shown(l, "zzz", {}).empty());
    // ...a word naming a pane puts the panes first; otherwise the actions come first.
    const std::vector<std::size_t> term = pal::shown(l, "term", {});
    REQUIRE(term.size() == 2);
    CHECK(l.entries[term[0]].kind == kCommandPane);
    CHECK(l.entries[pal::shown(l, "", {})[0]].kind == kCommandAction);
    // WHAT WAS CHOSEN THIS RUN COMES FIRST, the most recent first, and the history is bounded.
    std::vector<std::string> history = pal::remembered({}, l.entries[7].address());
    history = pal::remembered(history, l.entries[2].address());
    const std::vector<std::size_t> all = pal::shown(l, "", history);
    CHECK(all[0] == 2);
    CHECK(all[1] == 7);
    for (std::size_t i = 0; i < 40; ++i) history = pal::remembered(history, "x" + std::to_string(i));
    CHECK(history.size() == pal::kMaxHistory);

    // THE BOUND: entries past it are left out, and said.
    PaneInventory many;
    for (std::size_t i = 0; i < pal::kMaxEntries + 5; ++i)
        many.panes.push_back({"zengine.test." + std::to_string(i), "p", "P", "", false, true, false});
    const pal::Listing big = pal::entries_of(KeymapShown{}, many);
    CHECK(big.entries.size() == pal::kMaxEntries);
    CHECK(big.left_out == 5);
    CHECK(big.left_from == "panes");

    // THE KEY THAT OPENS IT TYPES NO SPACE: a window that types one with it has it swallowed.
    CHECK(expected_text_of(input::scan::kSpace, input::mod::kCtrl) == " ");
}

TEST_CASE("WL-DESK-18: ctrl+space shows the palette over the pane holding the keys, its Return runs "
          "the entry chosen by its key's path as that hand, and Escape puts it down with the keys "
          "given back") {
    PaneRig r;
    ProviderSeat* tools = nullptr;
    start(r, tools);
    const loom::WeaveId palette = r.load("zengine-palette", WORKSHOP_SO_PALETTE, kPaletteRole);
    REQUIRE(palette.valid());
    REQUIRE(r.load_refusals.empty());
    const std::int64_t alpha = kind_of(r, kTools, "alpha");

    // THE KEY: the palette seated over Alpha, holding the keys.
    r.key(input::scan::kSpace, input::mod::kCtrl);
    REQUIRE(on_desk(r, kPaletteRole, kPalettePane));
    const std::int64_t shown = kind_of(r, kPaletteRole, kPalettePane);
    CHECK(r.session().panes.keyboard == shown);

    // A PANE'S ROW, bound to no key: the palette put down, the keys back on Alpha, and the row sent
    // to Alpha as its key would send it.
    const std::size_t asked = tools->actions.size();
    r.text("poke");
    r.key(input::scan::kReturn);
    CHECK_FALSE(on_desk(r, kPaletteRole, kPalettePane));
    REQUIRE(tools->actions.size() == asked + 1);
    CHECK(tools->actions.back().pane == "alpha");
    CHECK(tools->actions.back().id == "alpha.poke");
    CHECK(r.session().panes.keyboard == alpha);

    // ESCAPE: put down with nothing run, the keys back where they were.
    r.key(input::scan::kSpace, input::mod::kCtrl);
    REQUIRE(on_desk(r, kPaletteRole, kPalettePane));
    const std::size_t before = tools->actions.size();
    r.key(input::scan::kEscape);
    CHECK_FALSE(on_desk(r, kPaletteRole, kPalettePane));
    CHECK(r.session().panes.keyboard == alpha);
    CHECK(tools->actions.size() == before);
    // ...AND THE KEY AGAIN, over the open palette, hides it.
    r.key(input::scan::kSpace, input::mod::kCtrl);
    REQUIRE(on_desk(r, kPaletteRole, kPalettePane));
    r.key(input::scan::kSpace, input::mod::kCtrl);
    CHECK_FALSE(on_desk(r, kPaletteRole, kPalettePane));

    // A PANE: shown and focused as a launch does.
    REQUIRE_FALSE(on_desk(r, kTools, "beta"));
    r.key(input::scan::kSpace, input::mod::kCtrl);
    r.text("beta");
    r.key(input::scan::kReturn);
    CHECK(on_desk(r, kTools, "beta"));
    CHECK(r.session().panes.keyboard == kind_of(r, kTools, "beta"));

    // WORKSHOP'S OWN ROW, by its key in command mode, the keys Workshop's.
    const bool titles = r.session().pane_titles;
    r.key(input::scan::kSpace, input::mod::kCtrl);
    r.text("titles");
    r.key(input::scan::kReturn);
    CHECK(r.session().pane_titles != titles);
    CHECK(r.session().panes.keyboard == kNoPaneKind);

    // AN APPLICATION ROW, asked of the desktop as its key asks it: the Hotkeys pane shown.
    REQUIRE_FALSE(on_desk(r, kDesktopRole, kHotkeysPane));
    r.key(input::scan::kSpace, input::mod::kCtrl);
    REQUIRE(on_desk(r, kPaletteRole, kPalettePane));
    r.text("desktop.hotkeys");
    r.key(input::scan::kReturn);
    CHECK_FALSE(on_desk(r, kPaletteRole, kPalettePane));
    CHECK(on_desk(r, kDesktopRole, kHotkeysPane));
}

TEST_CASE("WL-DESK-18: a run is refused under any number but the palette's own unspent row and "
          "from any office but the palette's, and nothing is run") {
    PaneRig r;
    ProviderSeat* tools = nullptr;
    start(r, tools);
    const Asker own = mount_asker(r, kPaletteRole);
    const Asker stranger = mount_asker(r, "zengine.test.stranger");
    drive(r, own, [](CommandAsker& a, loom::Mail& m) {
        (void)m.as_role(a.office()).send_to_role(
            kWorkshopProvider, v2::PaneOffered{kPalettePane, "Palette", "a stand-in", 10, 40});
        v3::PaneActions rows;
        rows.pane = kPalettePane;
        rows.rows.push_back(v3::PaneActionRow{"palette.run", "run", input::scan::kReturn,
                                              input::mod::kNone, "", {}});
        (void)m.as_role(a.office()).send_to_role(kWorkshopProvider, rows);
    });
    focus(r, kPaletteRole, kPalettePane);
    REQUIRE(r.session().panes.keyboard == kind_of(r, kPaletteRole, kPalettePane));
    r.key(input::scan::kReturn);
    REQUIRE(own.seat->asks.size() == 1);
    const std::uint64_t row = own.seat->asks.back();
    const CommandRunRequested beta{kCommandPane, kTools, "beta", ""};

    // ANOTHER NUMBER: refused, in words, and Beta not shown.
    drive(r, own, [&](CommandAsker& a, loom::Mail& m) { a.say(m, beta, row + 1000); });
    REQUIRE(own.seat->answers.size() == 1);
    CHECK_FALSE(own.seat->answers.back().ran);
    CHECK(own.seat->answers.back().refusal.find("palette's own row") != std::string::npos);
    CHECK_FALSE(on_desk(r, kTools, "beta"));
    // ANOTHER OFFICE, under the palette's own number: not even answered, and the number unspent.
    drive(r, stranger, [&](CommandAsker& a, loom::Mail& m) { a.say(m, beta, row); });
    CHECK(stranger.seat->answers.empty());
    CHECK_FALSE(on_desk(r, kTools, "beta"));
    // ITS OWN NUMBER: run, once.
    drive(r, own, [&](CommandAsker& a, loom::Mail& m) { a.say(m, beta, row); });
    REQUIRE(own.seat->answers.size() == 2);
    CHECK(own.seat->answers.back().ran);
    CHECK(on_desk(r, kTools, "beta"));
    CHECK_FALSE(on_desk(r, kPaletteRole, kPalettePane));
    // ...AND SPENT: the same number again runs nothing.
    drive(r, own, [&](CommandAsker& a, loom::Mail& m) {
        a.say(m, CommandRunRequested{kCommandPane, kTools, "alpha", ""}, row);
    });
    REQUIRE(own.seat->answers.size() == 3);
    CHECK_FALSE(own.seat->answers.back().ran);
}
