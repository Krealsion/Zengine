// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the quick setup (WL-FIRST): the real `zengine-first-run` image
// loaded beside Workshop, shown once at a start that keeps settings and never where none are kept;
// Done writing `answered` and putting the pane down; the Neovim box asking a stand-in editor switch
// and following its answers; the lines it draws and the item each press chooses; the grant
// Workshop admits the office with; and on a weaver's host, the weaver's hand alone reaching it. One
// source of the panes program, run in `workshop_panes_hand`.

#include "doctest.h"

#include "workshop_support.hpp"
#include "census.hpp"
#include "guest_hand.hpp"

#include "workshop/admission.hpp"
#include "workshop/editor_switch_vocabulary.hpp"
#include "workshop/first-run/setup.hpp"
#include "workshop/first-run/story.hpp"

#include "launcher/vocabulary.hpp"
#include "view/vocabulary.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fr = zengine::first_run;

/// A STAND-IN FOR THE EDITOR SWITCH, in its office: it records each ask and answers it as a case
/// says -- the status, asked quietly, with the choice it holds, a switch with what `on_switch`
/// returns.
class SwitchStandIn
    : public loom::WeaveBase<SwitchStandIn, SeatState,
                             loom::Accept<EditorSwitchRequested, EditorSwitchConfirmed,
                                          EditorSwitchQuietStatusRequested>,
                             loom::Emit<EditorSwitchAnswered>> {
public:
    std::string active = fr::kStandardChoice;
    std::vector<std::string> asked; ///< `request <destination>` or `confirm <op> <consent>`
    std::function<EditorSwitchAnswered(const EditorSwitchRequested&)> on_switch;
    bool hold = false; ///< a switch asked is answered never: it stays under way
    int quiet = 0;     ///< the quiet status asks heard

    void on(const EditorSwitchQuietStatusRequested&, loom::Mail& mail) {
        ++quiet;
        EditorSwitchAnswered a;
        a.outcome = switch_outcome::kStatus;
        a.active = active;
        (void)mail.answer(a);
    }
    void on(const EditorSwitchRequested& r, loom::Mail& mail) {
        asked.push_back("request " + r.destination);
        if (hold) {
            return;
        }
        EditorSwitchAnswered a;
        if (on_switch) {
            a = on_switch(r);
        } else {
            a.outcome = switch_outcome::kSwitched;
            a.destination = r.destination;
        }
        if (a.outcome == switch_outcome::kSwitched) {
            active = r.destination;
        }
        (void)mail.answer(a);
    }
    void on(const EditorSwitchConfirmed& c, loom::Mail& mail) {
        asked.push_back("confirm " + std::to_string(c.op) + " " + c.consent);
        active = fr::kNeovimChoice;
        EditorSwitchAnswered a;
        a.op = c.op;
        a.outcome = switch_outcome::kSwitched;
        a.destination = fr::kNeovimChoice;
        (void)mail.answer(a);
    }
};

SwitchStandIn* mount_switch(PaneRig& r) {
    auto weave = std::make_unique<SwitchStandIn>();
    SwitchStandIn* raw = weave.get();
    loom::Grant grant;
    grant.allow_to_any(EditorSwitchAnswered::zen_name, EditorSwitchAnswered::zen_version);
    const loom::WeaveId id = r.bus.register_weave(std::move(weave), std::move(grant),
                                                  std::string(kEditorSwitchRole));
    raw->zen_set_self(id);
    return raw;
}

bool on_desk(PaneRig& r) {
    return has_pane(r.session().setup.active, PaneRef{fr::kFirstRunRole, fr::kSetupPane});
}

/// A START: Workshop keeping its settings at `prefs` (none: an isolated run), the switch's
/// stand-in holding the office with `active`, and the real quick setup loaded.
SwitchStandIn* start(PaneRig& r, const std::string& prefs,
                     const std::string& active = fr::kStandardChoice) {
    r.host.prefs_path = prefs;
    r.mount_workshop();
    r.ready();
    r.extent(160, 60);
    SwitchStandIn* sw = mount_switch(r);
    sw->active = active;
    REQUIRE(r.load("zengine-first-run", WORKSHOP_SO_FIRST_RUN, fr::kFirstRunRole).valid());
    REQUIRE(r.load_refusals.empty());
    return sw;
}

void act(PaneRig& r, const char* id) {
    CHECK(r.workshop_action(fr::kFirstRunRole, fr::kSetupPane, id).delivered);
}

/// THE WORDS A GUEST'S REFUSAL NAMES THE QUICK SETUP IN, as `WorkshopWeave::refused_toward` says.
constexpr const char* kSetupRefused =
    "the quick setup, which switches the editor and writes the settings file, answers only the "
    "weaver's hand";

std::int64_t setup_kind(PaneRig& r) {
    const RuntimePane* held = r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane);
    REQUIRE(held != nullptr);
    return held->kind;
}

/// A CELL OF THE ROOM NO PANE HOLDS: a press there takes the keys from every pane.
std::pair<std::int64_t, std::int64_t> free_room_cell(PaneRig& r) {
    const Screen sc = screen_of(r.session());
    const ScreenCells cells = cells_of(sc);
    for (std::int64_t y = cells.room_h - 1; y >= 0; --y) {
        for (std::int64_t x = cells.room_w - 1; x >= 0; --x) {
            if (!occupied_at(r.session().panes, r.session().setup.active, sc, x, y).occupied) {
                return {x, y};
            }
        }
    }
    FAIL("this screen has no empty room to press");
    return {0, 0};
}

/// THE QUICK SETUP'S CENSUS: the three rows of each of its two panes, in no mode.
const CensusDriverOf kFirstRunCensus{fr::kFirstRunRole, [] {
    OfficeCensus census;
    TempDir dir("first-run-census");
    PaneRig r;
    (void)start(r, dir.file("prefs.json"));
    for (const char* pane : {fr::kSetupPane, fr::kStoryPane}) {
        const RuntimePane* held = r.session().panes.runtime.find(fr::kFirstRunRole, pane);
        REQUIRE(held != nullptr);
        census_take(census, fr::kFirstRunRole, *held);
    }
    return census;
}};

/// THE QUICK SETUP'S PRESSES: each row on a quick setup made afresh.
const PressDriverOf kFirstRunPresses{fr::kFirstRunRole, [](PressWitness& w) {
    std::vector<v4::PaneActionRow> rows;
    {
        TempDir dir("first-run-rows");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        rows = r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane)->actions;
    }
    for (const v4::PaneActionRow& row : rows) {
        TempDir dir("first-run-press");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        w.judge(r, fr::kSetupPane, "", row);
    }
    // ...AND "START HERE"'S, on one opened afresh by Done.
    std::vector<v4::PaneActionRow> story_rows;
    {
        TempDir dir("first-run-story-rows");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        story_rows = r.session().panes.runtime.find(fr::kFirstRunRole, fr::kStoryPane)->actions;
    }
    for (const v4::PaneActionRow& row : story_rows) {
        TempDir dir("first-run-story-press");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        for (const char* id : {fr::kActionDown, fr::kActionDown, fr::kActionChoose}) act(r, id);
        w.judge(r, fr::kStoryPane, "", row);
    }
}};

} // namespace

TEST_CASE("WL-FIRST-01: the quick setup shows itself once at a start that keeps settings and has "
          "not answered, and never where none are kept") {
    SUBCASE("a start keeping a settings file: shown in front, holding the keys") {
        TempDir dir("first-run-shown");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        REQUIRE(on_desk(r));
        const RuntimePane* held = r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane);
        REQUIRE(held != nullptr);
        CHECK(r.session().panes.keyboard == held->kind);
    }
    SUBCASE("a pane a hand opened before the setup arrived keeps the keys: not shown by itself") {
        TempDir dir("first-run-late");
        PaneRig r;
        r.host.prefs_path = dir.file("prefs.json");
        r.mount_workshop();
        r.ready();
        r.extent(160, 60);
        (void)mount_switch(r);
        const char* tools = "zengine.test.tools";
        ProviderSeat* seat = r.mount_provider(tools);
        r.drive(seat, [](ProviderSeat& s, loom::Mail& m) {
            s.offer(m, PaneOffered{"alpha", "Alpha", "a fixture"});
        });
        r.pick(PaneRef{tools, "alpha"});
        REQUIRE(has_pane(r.session().setup.active, PaneRef{tools, "alpha"}));
        REQUIRE(r.load("zengine-first-run", WORKSHOP_SO_FIRST_RUN, fr::kFirstRunRole).valid());
        CHECK_FALSE(on_desk(r));
        CHECK(r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane) != nullptr);
        CHECK(has_pane(r.session().setup.active, PaneRef{tools, "alpha"}));
    }
    SUBCASE("an isolated run keeps no settings: nothing shown by itself, the pane still offered") {
        PaneRig r;
        (void)start(r, std::string());
        CHECK_FALSE(on_desk(r));
        CHECK(r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane) != nullptr);
    }
}

TEST_CASE("WL-FIRST-02: Done writes answered and puts the pane down, and the next start shows "
          "nothing") {
    TempDir dir("first-run-done");
    const std::string prefs = dir.file("prefs.json");
    {
        PaneRig r;
        (void)start(r, prefs);
        REQUIRE(on_desk(r));
        act(r, fr::kActionDown); // the box, The story, then Done
        act(r, fr::kActionDown);
        act(r, fr::kActionChoose);
        CHECK_FALSE(on_desk(r));
        CHECK(slurp(prefs).find("\"answered\"") != std::string::npos);
        CHECK(slurp(prefs).find(fr::kFirstRunRole) != std::string::npos);
    }
    PaneRig again;
    (void)start(again, prefs);
    CHECK_FALSE(on_desk(again));
}

TEST_CASE("WL-FIRST-03: the Neovim box asks the editor switch, shows its answer in its words, and "
          "confirms a consent only when chosen again") {
    TempDir dir("first-run-switch");
    PaneRig r;
    SwitchStandIn* sw = start(r, dir.file("prefs.json"));
    REQUIRE(on_desk(r));
    // THE FIRST ASK WOULD LOSE SOMETHING: the switch asks first, and nothing is confirmed yet.
    sw->on_switch = [](const EditorSwitchRequested& asked) {
        EditorSwitchAnswered a;
        a.op = 7;
        a.outcome = switch_outcome::kNeedsConfirmation;
        a.destination = asked.destination;
        a.consent = "digest-7";
        a.losses = {"unsaved changes in notes.txt"};
        return a;
    };
    // THE BOX WAS READ FROM THE SWITCH'S STATUS, ASKED QUIETLY: the box shows it, so nothing of it
    // is said in Notices.
    CHECK(sw->quiet > 0);
    act(r, fr::kActionChoose);
    CHECK(sw->asked == std::vector<std::string>{"request neovim"});
    // ...CHOSEN AGAIN, the consent is confirmed under the switch's own op and digest.
    act(r, fr::kActionChoose);
    CHECK(sw->asked == std::vector<std::string>{"request neovim", "confirm 7 digest-7"});
    CHECK(sw->active == fr::kNeovimChoice);
    // THE BOX FOLLOWS THE SWITCH: checked now, so choosing it asks for the standard Editor back.
    sw->on_switch = nullptr;
    act(r, fr::kActionChoose);
    CHECK(sw->asked.back() == "request standard");
    CHECK(sw->active == fr::kStandardChoice);
    // A REFUSAL IS THE SWITCH'S, and the box is read again from its status.
    sw->on_switch = [](const EditorSwitchRequested& asked) {
        EditorSwitchAnswered a;
        a.outcome = switch_outcome::kRefused;
        a.destination = asked.destination;
        a.detail = "no `nvim` found";
        return a;
    };
    act(r, fr::kActionChoose);
    CHECK(sw->asked.back() == "request neovim");
    CHECK(sw->active == fr::kStandardChoice);
    sw->on_switch = nullptr;
    act(r, fr::kActionChoose);
    CHECK(sw->asked.back() == "request neovim");

    // THE BOX IS READ FROM THE SWITCH AT ACTIVATION: a start where Neovim already holds the office
    // (the session remembered it) shows the box checked, so choosing it asks for the standard one.
    {
        TempDir again_dir("first-run-switch-neovim");
        PaneRig again;
        SwitchStandIn* held = start(again, again_dir.file("prefs.json"), fr::kNeovimChoice);
        act(again, fr::kActionChoose);
        CHECK(held->asked == std::vector<std::string>{"request standard"});
    }

    // AND THE OFFICE IS ADMITTED WITH THE SWITCH'S FOUR ASKS, ITS STATUS THE QUIET ONE, TO THE
    // SWITCH'S OFFICE ALONE.
    loom::AdmissionRequest speak;
    speak.stage = loom::AdmissionStage::Speak;
    speak.role = fr::kFirstRunRole;
    const loom::AdmissionVerdict v = artifact_admission()(speak);
    REQUIRE(v.admitted);
    for (const auto& [name, version] :
         std::vector<std::pair<std::string, std::uint32_t>>{
             {EditorSwitchRequested::zen_name, EditorSwitchRequested::zen_version},
             {EditorSwitchConfirmed::zen_name, EditorSwitchConfirmed::zen_version},
             {EditorSwitchCancelled::zen_name, EditorSwitchCancelled::zen_version},
             {EditorSwitchQuietStatusRequested::zen_name, EditorSwitchQuietStatusRequested::zen_version}}) {
        CAPTURE(name);
        CHECK(v.grant.permits_role(name, version, kEditorSwitchRole));
        CHECK_FALSE(v.grant.permits_role(name, version, kWorkshopProvider));
    }
    CHECK_FALSE(v.grant.permits_role(EditorSwitchStatusRequested::zen_name,
                                     EditorSwitchStatusRequested::zen_version, kEditorSwitchRole));
    // ...its pictures only to the windows, never to Workshop; and no load.
    CHECK_FALSE(v.grant.permits_role(zengine::graphics::Drawn::zen_name, zengine::graphics::Drawn::zen_version,
                                     kWorkshopProvider));
    CHECK(v.grant.permits_role(zengine::graphics::Drawn::zen_name, zengine::graphics::Drawn::zen_version,
                               std::string(zengine::graphics::kWindowOfficePrefix) + zengine::graphics::kSdlWindow));
    CHECK_FALSE(v.grant.permits_role(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version,
                                     "zen.manager"));
}

TEST_CASE("WL-FIRST-02: the quick setup's lines name the box, the setups the launcher lists with "
          "Start on each, The story, the pages and Done and Skip, each press choosing its own item") {
    fr::Moment m;
    const std::vector<fr::Line> lines = fr::lines_of(m);
    std::vector<fr::Item> items;
    bool setups = false, started = false;
    for (const fr::Line& l : lines) {
        if (l.item != fr::Item::kNone) items.push_back(l.item);
        setups = setups || l.text.find(fr::kSetupsPage) != std::string::npos;
        started = started || l.text.find(fr::kGettingStartedPage) != std::string::npos;
    }
    CHECK(items == std::vector<fr::Item>{fr::Item::kNeovim, fr::Item::kStory, fr::Item::kDone,
                                         fr::Item::kSkip});
    CHECK(setups);
    CHECK(started);
    // THE SETUPS THE LAUNCHER LISTED, each a Start of its own between the box and The story.
    m.setups = {{"values", "Value inspection", "Inventory and Info"}, {"commands", "", ""}};
    std::vector<fr::Item> listed;
    std::vector<std::string> starts;
    for (const fr::Line& l : fr::lines_of(m)) {
        if (l.item != fr::Item::kNone) listed.push_back(l.item);
        if (fr::setup_of(l.item) >= 0) starts.push_back(l.text);
    }
    CHECK(listed == std::vector<fr::Item>{fr::Item::kNeovim, fr::setup_item(0), fr::setup_item(1),
                                          fr::Item::kStory, fr::Item::kDone, fr::Item::kSkip});
    REQUIRE(starts.size() == 2);
    CHECK(starts[0].find("[ Start ] Value inspection -- Inventory and Info") != std::string::npos);
    CHECK(starts[1].find("[ Start ] commands") != std::string::npos);
    CHECK(fr::stepped(m, fr::Item::kNeovim, 1) == fr::setup_item(0));
    CHECK(fr::stepped(m, fr::setup_item(1), 1) == fr::Item::kStory);
    m.setups.clear();
    // The box says what the switch said; the cursor is marked; the ends hold.
    m.neovim = true;
    m.switch_said = "Neovim edits text now";
    m.cursor = fr::Item::kDone;
    bool checked = false, said = false, marked = false;
    for (const fr::Line& l : fr::lines_of(m)) {
        checked = checked || l.text.find("[x] Edit text with Neovim") != std::string::npos;
        said = said || l.text.find("Neovim edits text now") != std::string::npos;
        marked = marked || (l.item == fr::Item::kDone && l.text.rfind("> ", 0) == 0);
    }
    CHECK(checked);
    CHECK(said);
    CHECK(marked);
    CHECK(fr::stepped(m, fr::Item::kNeovim, -1) == fr::Item::kNeovim);
    CHECK(fr::stepped(m, fr::Item::kSkip, 1) == fr::Item::kSkip);
    CHECK(fr::stepped(m, fr::Item::kNeovim, 1) == fr::Item::kStory);
}

TEST_CASE("WL-FIRST-02: a press the window landed on Done before a notice came under the items "
          "chooses Done: the items stand where they stood") {
    TempDir dir("first-run-notice-press");
    PaneRig r;
    DrawnNumbers drawn(r.bus);
    SwitchStandIn* sw = start(r, dir.file("prefs.json"));
    REQUIRE(on_desk(r));
    const std::int64_t kind = setup_kind(r);
    const std::int64_t before = drawn.of(r.session(), kind);
    REQUIRE(before != 0);
    std::int64_t done = -1;
    const std::vector<std::string> shown = pane_rows(r, kind);
    for (std::size_t i = 0; i < shown.size(); ++i) {
        if (shown[i].find("[ Done ]") != std::string::npos) done = static_cast<std::int64_t>(i);
    }
    REQUIRE(done >= 0);
    // THE SWITCH HOLDS ITS ANSWER: the box chosen again says so, under the items.
    sw->hold = true;
    act(r, fr::kActionChoose);
    act(r, fr::kActionChoose);
    const std::vector<std::string> noticed = pane_rows(r, kind);
    REQUIRE(std::any_of(noticed.begin(), noticed.end(), [](const std::string& row) {
        return row.find("the editor switch is still under way") != std::string::npos;
    }));
    REQUIRE(noticed[static_cast<std::size_t>(done)].find("[ Done ]") != std::string::npos);
    // ...AND A PRESS THE WINDOW LANDED ON DONE BEFORE IT, delivered now, chooses Done.
    press_as_drawn(r, kind, done, 4, "", before);
    CHECK_FALSE(on_desk(r));
}

TEST_CASE("WL-GUEST-06: on a weaver's host a guest's key and press toward the quick setup are "
          "refused, and the switch is not asked; on a development host they reach it") {
    for (const bool development : {false, true}) {
        CAPTURE(development);
        TempDir dir("first-run-guest");
        PaneRig r;
        SwitchStandIn* sw = start(r, dir.file("prefs.json"));
        REQUIRE(on_desk(r));
        const std::int64_t kind = setup_kind(r);
        REQUIRE(r.session().panes.keyboard == kind);
        guest_hand::GuestHand g(r, {"input"}, development);

        // A GUEST'S RETURN ON THE BOX, which would ask the editor switch.
        r.session().notice.clear();
        g.key(input::scan::kReturn);
        const std::string keyed = r.last_notice();
        if (development) {
            CHECK(sw->asked == std::vector<std::string>{"request neovim"});
        } else {
            CHECK_MESSAGE(keyed.find(kSetupRefused) != std::string::npos, keyed);
            CHECK(sw->asked.empty());
        }

        // THE WEAVER PRESSES THE EMPTY ROOM, and the setup no longer holds the keys...
        const auto [room_x, room_y] = free_room_cell(r);
        g.weaver_press_cell(room_x, room_y);
        REQUIRE(r.session().panes.keyboard != kind);
        // ...A GUEST'S PRESS ON ITS AREA takes them back only on a development host.
        const CellRect body = external_body_rect(r.session(), kind);
        r.session().notice.clear();
        g.press_cell(body.x, body.y);
        const std::string pressed = r.last_notice();
        if (development) {
            CHECK(r.session().panes.keyboard == kind);
        } else {
            CHECK_MESSAGE(pressed.find(kSetupRefused) != std::string::npos, pressed);
            CHECK(r.session().panes.keyboard != kind);
        }

        // THE WEAVER'S OWN HAND REACHES IT ON EITHER HOST.
        g.weaver_press_cell(body.x, body.y);
        CHECK(r.session().panes.keyboard == kind);
        const std::size_t asked = sw->asked.size();
        g.weaver_key(input::scan::kReturn);
        CHECK(sw->asked.size() == asked + 1);
    }
}

namespace {

/// A STAND-IN FOR THE LAUNCHER, in its office: it answers a listing with `entries`, records each
/// start it is asked for and answers it started, and says a start to any when a case drives it.
class LauncherStandIn
    : public loom::WeaveBase<LauncherStandIn, SeatState,
                             loom::Accept<zengine::launcher::LaunchCatalogRequested,
                                          zengine::launcher::Launch, SeatDo>,
                             loom::Emit<zengine::launcher::LaunchCatalog,
                                        zengine::launcher::Launched>> {
public:
    std::vector<zengine::launcher::LaunchEntry> entries;
    std::vector<std::string> started; ///< `<kind> <name>` of each start asked
    std::int64_t listings = 0;
    std::function<void(LauncherStandIn&, loom::Mail&)> next;

    void on(const zengine::launcher::LaunchCatalogRequested&, loom::Mail& mail) {
        ++listings;
        (void)mail.answer(zengine::launcher::LaunchCatalog{entries, 0, ""});
    }
    void on(const zengine::launcher::Launch& asked, loom::Mail& mail) {
        started.push_back(asked.kind + " " + asked.name);
        (void)mail.answer(zengine::launcher::Launched{true, asked.kind, asked.name, 4242, "started"});
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(LauncherStandIn&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
};

/// A STAND-IN FOR A RUNNING VIEW, in an office of its own (a view's office is its name): it offers
/// its pane `view` when a case drives it, and answers what its wiring has done with `wiring`, or
/// says it to any when driven.
class ViewStandIn
    : public loom::WeaveBase<ViewStandIn, SeatState,
                             loom::Accept<PaneCatalogRequested, zengine::view::ViewWiringRequested,
                                          SeatDo>,
                             loom::Emit<v2::PaneOffered, zengine::view::ViewWiring>> {
public:
    zengine::view::ViewWiring wiring;
    std::int64_t asked = 0;
    std::function<void(ViewStandIn&, loom::Mail&)> next;

    void on(const PaneCatalogRequested&, loom::Mail&) {}
    /// ...as a running view answers it: an ordinary send back to the asker, as its own office.
    void on(const zengine::view::ViewWiringRequested&, loom::Mail& mail) {
        ++asked;
        (void)mail.as_role(office).send(mail.sender(), wiring, mail.correlation());
    }
    std::string office;
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            std::function<void(ViewStandIn&, loom::Mail&)> once;
            once.swap(next);
            once(*this, mail);
        }
    }
};

template <class Seat>
Seat* mount_any(PaneRig& r, const std::string& office, loom::WeaveId* id_out = nullptr) {
    auto weave = std::make_unique<Seat>();
    Seat* raw = weave.get();
    const loom::WeaveId id = r.bus.register_weave(std::move(weave), loom::Grant{}.allow_any(), office);
    raw->zen_set_self(id);
    if (id_out != nullptr) *id_out = id;
    return raw;
}

template <class Seat>
void drive_any(PaneRig& r, loom::WeaveId id, Seat* seat, std::function<void(Seat&, loom::Mail&)> what) {
    seat->next = std::move(what);
    (void)r.bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
}

/// A RUNNING VIEW'S OFFICE, as the view host names one.
constexpr const char* kTallyPanel = "tally.panel";

bool story_on_desk(PaneRig& r) {
    return has_pane(r.session().setup.active, PaneRef{fr::kFirstRunRole, fr::kStoryPane});
}
std::int64_t kind_in(PaneRig& r, const char* office, const char* pane) {
    const RuntimePane* held = r.session().panes.runtime.find(office, pane);
    REQUIRE(held != nullptr);
    return held->kind;
}
bool rows_say(PaneRig& r, std::int64_t kind, const std::string& words) {
    for (const std::string& row : pane_rows(r, kind)) {
        if (row.find(words) != std::string::npos) return true;
    }
    return false;
}

/// A START as `start` makes one, the launcher's stand-in in its office first, so the quick setup's
/// listing is answered.
struct StoryStart {
    SwitchStandIn* sw = nullptr;
    LauncherStandIn* launcher = nullptr;
    loom::WeaveId launcher_id{};
};
StoryStart start_with_launcher(PaneRig& r, const std::string& prefs,
                               std::vector<zengine::launcher::LaunchEntry> entries = {}) {
    r.host.prefs_path = prefs;
    r.mount_workshop();
    r.ready();
    r.extent(160, 60);
    StoryStart s;
    s.sw = mount_switch(r);
    s.launcher = mount_any<LauncherStandIn>(r, zengine::launcher::kLauncherRole, &s.launcher_id);
    s.launcher->entries = std::move(entries);
    REQUIRE(r.load("zengine-first-run", WORKSHOP_SO_FIRST_RUN, fr::kFirstRunRole).valid());
    REQUIRE(r.load_refusals.empty());
    return s;
}

/// The quick setup answered with Done, from the box: through The story's line, then Done.
void answer_done(PaneRig& r, std::size_t setups = 0) {
    for (std::size_t i = 0; i < setups + 2; ++i) act(r, fr::kActionDown);
    act(r, fr::kActionChoose);
}

std::string story_kept(const std::string& prefs) {
    const std::string text = slurp(prefs);
    const std::size_t key = text.find("\"story\"");
    if (key == std::string::npos) return std::string(); // never written: no step ticked
    const std::size_t open = text.find('"', text.find(':', key) + 1);
    const std::size_t close = text.find('"', open + 1);
    return text.substr(open + 1, close - open - 1);
}

} // namespace

TEST_CASE("WL-FIRST-04: each step is ticked by the fact its owner says -- the inventory, a running "
          "view's wiring, the launcher's listing and its start -- and kept; a view's wiring said "
          "by an office no running view holds ticks nothing") {
    // MUTATION: the wiring believed from any office -- a stranger's ViewWiring ticks steps 2 and 3.
    TempDir dir("first-run-facts");
    const std::string prefs = dir.file("prefs.json");
    PaneRig r;
    StoryStart s = start_with_launcher(r, prefs);
    answer_done(r);
    REQUIRE(story_on_desk(r));
    const std::int64_t story = kind_in(r, fr::kFirstRunRole, fr::kStoryPane);
    CHECK(story_kept(prefs) == "");

    zengine::view::ViewWiring wired;
    wired.intents.push_back({"count", "Tally", 1, true, 1, 0, "", true});
    wired.fields.push_back({"total", "TallyTotal", 1, "total", true});
    // A STRANGER SAYS A WIRING: no running view holds its office, so nothing is ticked.
    loom::WeaveId stranger_id{};
    ViewStandIn* stranger = mount_any<ViewStandIn>(r, "zengine.stranger", &stranger_id);
    drive_any<ViewStandIn>(r, stranger_id, stranger, [&wired](ViewStandIn&, loom::Mail& m) {
        (void)m.as_role("zengine.stranger").publish(wired);
    });
    CHECK(story_kept(prefs) == "");

    // A RUNNING VIEW COMES ON THE DESK: step 1 by the inventory, and the view is asked its wiring,
    // which ticks 2 and 3 -- and the launcher, asked again, lists an app: step 4.
    s.launcher->entries = {{"app", "tally", "tally", "an app", false}};
    loom::WeaveId view_id{};
    ViewStandIn* tally = mount_any<ViewStandIn>(r, kTallyPanel, &view_id);
    tally->office = kTallyPanel;
    tally->wiring = wired;
    drive_any<ViewStandIn>(r, view_id, tally, [](ViewStandIn&, loom::Mail& m) {
        (void)m.as_role(kTallyPanel).send_to_role(kWorkshopProvider,
                                                  v2::PaneOffered{"view", "Tally", "the panel", 10, 30});
    });
    CHECK(tally->asked >= 1);
    CHECK(story_kept(prefs) == "1 2 3 4");
    CHECK(rows_say(r, story, "[x] 1. Make a view -- done"));
    CHECK(rows_say(r, story, "[ ] 5. Start it"));

    // THE LAUNCHER SAYS TO ANY THAT IT STARTED THE APP: step 5.
    drive_any<LauncherStandIn>(r, s.launcher_id, s.launcher, [](LauncherStandIn&, loom::Mail& m) {
        (void)m.as_role(zengine::launcher::kLauncherRole)
            .publish(zengine::launcher::Launched{true, "app", "tally", 4242, "started"});
    });
    CHECK(story_kept(prefs) == "1 2 3 4 5");
    CHECK(rows_say(r, story, "The story is done"));
}

TEST_CASE("WL-FIRST-05: Start here opens when the quick setup closes, holding the keys, and at "
          "each later start until the story is done beside what holds the keys, never taking them") {
    // MUTATION: a later start launching Start here as the first does -- it takes the keys.
    TempDir dir("first-run-story-shown");
    const std::string prefs = dir.file("prefs.json");
    {
        PaneRig r;
        (void)start_with_launcher(r, prefs);
        REQUIRE(on_desk(r));
        CHECK_FALSE(story_on_desk(r));
        answer_done(r);
        CHECK_FALSE(on_desk(r));
        REQUIRE(story_on_desk(r));
        CHECK(r.session().panes.keyboard == kind_in(r, fr::kFirstRunRole, fr::kStoryPane));
    }
    {
        PaneRig again;
        (void)start_with_launcher(again, prefs);
        CHECK_FALSE(on_desk(again));
        REQUIRE(story_on_desk(again));
        CHECK(again.session().panes.keyboard != kind_in(again, fr::kFirstRunRole, fr::kStoryPane));
        // EVERY BOX TICKED BY HAND: the story is done.
        for (std::size_t step = 0; step < fr::kSteps; ++step) {
            if (step > 0) {
                CHECK(again.workshop_action(fr::kFirstRunRole, fr::kStoryPane, fr::kStoryDown).delivered);
                CHECK(again.workshop_action(fr::kFirstRunRole, fr::kStoryPane, fr::kStoryDown).delivered);
            }
            CHECK(again.workshop_action(fr::kFirstRunRole, fr::kStoryPane, fr::kStoryChoose).delivered);
        }
        CHECK(story_kept(prefs) == "1h 2h 3h 4h 5h");
    }
    PaneRig done;
    (void)start_with_launcher(done, prefs);
    CHECK_FALSE(on_desk(done));
    CHECK_FALSE(story_on_desk(done));
    CHECK(done.session().panes.runtime.find(fr::kFirstRunRole, fr::kStoryPane) != nullptr);
}

TEST_CASE("WL-FIRST-04: a box ticked by the weaver's hand says so and is undone by it, its owner's "
          "fact makes it the owner's, and Show me opens the step's pane and says its next move") {
    // MUTATION: a hand's tick kept over the owner's fact -- the box still says "ticked by you".
    TempDir dir("first-run-hand");
    const std::string prefs = dir.file("prefs.json");
    PaneRig r;
    (void)start_with_launcher(r, prefs);
    answer_done(r);
    REQUIRE(story_on_desk(r));
    const std::int64_t story = kind_in(r, fr::kFirstRunRole, fr::kStoryPane);
    const auto story_act = [&r](const char* id) {
        CHECK(r.workshop_action(fr::kFirstRunRole, fr::kStoryPane, id).delivered);
    };
    story_act(fr::kStoryChoose);
    CHECK(story_kept(prefs) == "1h");
    CHECK(rows_say(r, story, "[x] 1. Make a view -- ticked by you"));
    story_act(fr::kStoryChoose);
    CHECK(story_kept(prefs) == "");
    story_act(fr::kStoryChoose);
    CHECK(story_kept(prefs) == "1h");
    // THE INVENTORY SAYS A VIEW RUNS: the box is the owner's now, and a press leaves it.
    loom::WeaveId view_id{};
    ViewStandIn* tally = mount_any<ViewStandIn>(r, kTallyPanel, &view_id);
    tally->office = kTallyPanel;
    drive_any<ViewStandIn>(r, view_id, tally, [](ViewStandIn&, loom::Mail& m) {
        (void)m.as_role(kTallyPanel).send_to_role(kWorkshopProvider,
                                                  v2::PaneOffered{"view", "Tally", "the panel", 10, 30});
    });
    CHECK(story_kept(prefs) == "1");
    CHECK(rows_say(r, story, "[x] 1. Make a view -- done"));
    story_act(fr::kStoryChoose);
    CHECK(story_kept(prefs) == "1");
    CHECK(rows_say(r, story, "was ticked when a running view is on the desk"));

    // SHOW ME on step 1: the View Builder shown and given the keys, and the next move said.
    ProviderSeat* builder = r.mount_provider("zengine.view.builder");
    r.drive(builder, [](ProviderSeat& p, loom::Mail& m) {
        p.offer(m, PaneOffered{"view-builder", "View Builder", "a stand-in"});
    });
    story_act(fr::kStoryDown);
    story_act(fr::kStoryChoose);
    REQUIRE(has_pane(r.session().setup.active, PaneRef{"zengine.view.builder", "view-builder"}));
    CHECK(r.session().panes.keyboard == kind_in(r, "zengine.view.builder", "view-builder"));
    CHECK(rows_say(r, story, "Show me: place a number field"));
}

TEST_CASE("WL-FIRST-02: the quick setup lists the setups the launcher lists, and a Start asks "
          "Workshop for the hand that pressed, then the launcher, saying what it answered") {
    // MUTATION: a Start sent to the launcher before Workshop approved the hand -- a guest's press
    // on a weaver's host starts the setup.
    TempDir dir("first-run-setups");
    PaneRig r;
    StoryStart s = start_with_launcher(r, dir.file("prefs.json"),
                                       {{"demo", "values", "Value inspection", "Inventory and Info", false},
                                        {"app", "tally", "tally", "an app", false}});
    REQUIRE(on_desk(r));
    const std::int64_t setup = setup_kind(r);
    CHECK(rows_say(r, setup, "[ Start ] Value inspection -- Inventory and Info"));
    CHECK_FALSE(rows_say(r, setup, "[ Start ] tally"));
    // THE WEAVER'S KEYS: down to the setup, Return.
    r.key(input::scan::kDown);
    r.key(input::scan::kReturn);
    CHECK(s.launcher->started == std::vector<std::string>{"demo values"});
    CHECK(rows_say(r, setup, "started the setup `values`, process 4242"));
    // A START UNDER NO KEY WORKSHOP NUMBERED is refused by Workshop, and nothing starts.
    act(r, fr::kActionChoose);
    CHECK(s.launcher->started.size() == 1);
    CHECK(rows_say(r, setup, "nothing was started --"));

    // NO LAUNCHER IN THIS WORKSHOP: the setups say so.
    TempDir bare_dir("first-run-no-launcher");
    PaneRig bare;
    (void)start(bare, bare_dir.file("prefs.json"));
    REQUIRE(on_desk(bare));
    CHECK(rows_say(bare, setup_kind(bare), "no launcher answers in this Workshop"));

    // AND THE GRANT NAMES EACH NEW ASK TO ITS OWNER ALONE: the listing and a start to the
    // launcher, the approval and a launch of either version to Workshop, the wiring's ask to any.
    loom::AdmissionRequest speak;
    speak.stage = loom::AdmissionStage::Speak;
    speak.role = fr::kFirstRunRole;
    const loom::AdmissionVerdict v = artifact_admission()(speak);
    REQUIRE(v.admitted);
    namespace la = zengine::launcher;
    CHECK(v.grant.permits_role(la::Launch::zen_name, la::Launch::zen_version, la::kLauncherRole));
    CHECK_FALSE(v.grant.permits_role(la::Launch::zen_name, la::Launch::zen_version, kWorkshopProvider));
    CHECK(v.grant.permits_role(la::LaunchCatalogRequested::zen_name,
                               la::LaunchCatalogRequested::zen_version, la::kLauncherRole));
    CHECK_FALSE(v.grant.permits_role(la::LaunchCatalogRequested::zen_name,
                                     la::LaunchCatalogRequested::zen_version, "zengine.stranger"));
    CHECK(v.grant.permits_role(v2::PaneOperationRequested::zen_name,
                               v2::PaneOperationRequested::zen_version, kWorkshopProvider));
    CHECK_FALSE(v.grant.permits_role(v2::PaneOperationRequested::zen_name,
                                     v2::PaneOperationRequested::zen_version, la::kLauncherRole));
    CHECK(v.grant.permits_role(v2::PaneLaunchRequested::zen_name, v2::PaneLaunchRequested::zen_version,
                               kWorkshopProvider));
    CHECK(v.grant.permits_role(zengine::view::ViewWiringRequested::zen_name,
                               zengine::view::ViewWiringRequested::zen_version, kTallyPanel));
    CHECK_FALSE(v.grant.permits_role(zengine::view::ViewWiring::zen_name,
                                     zengine::view::ViewWiring::zen_version, kTallyPanel));
}

namespace {
/// A PANE'S WORDS AS THE DESK SHOWS THEM, its rows read in order as one text: each row's leading and
/// trailing blanks dropped and the rows joined by a space, so a line carried on to the row beneath
/// reads whole, and one cut at the pane's edge does not.
std::string read_whole(PaneRig& r, std::int64_t kind) {
    std::string out;
    for (const std::string& row : pane_rows(r, kind)) {
        const std::size_t first = row.find_first_not_of(' ');
        const std::size_t last = row.find_last_not_of(' ');
        if (first == std::string::npos) continue;
        out += (out.empty() ? "" : " ") + row.substr(first, last - first + 1);
    }
    return out;
}
} // namespace

TEST_CASE("WL-FIRST-02: every line of the quick setup and of Start here is read whole at the pane's "
          "own size, a line wider than the pane going on in the rows beneath it") {
    // MUTATION: the lines left as wide as they are written -- the desk cuts each at the pane's
    // edge, and what ticks a step and a setup's line lose their ends.
    TempDir dir("first-run-whole");
    const std::string line = "Inventory, Info and a Reset button with nine captured Input structures, "
                             "and nothing else on the desk";
    PaneRig r;
    (void)start_with_launcher(r, dir.file("prefs.json"),
                              {{"demo", "values", "Value inspection", line, false}});
    REQUIRE(on_desk(r));
    const std::string setup = read_whole(r, setup_kind(r));
    CHECK_MESSAGE(setup.find("[ Start ] Value inspection -- " + line) != std::string::npos, setup);
    answer_done(r, 1);
    REQUIRE(story_on_desk(r));
    const std::string story = read_whole(r, kind_in(r, fr::kFirstRunRole, fr::kStoryPane));
    for (const fr::Step& step : fr::steps()) {
        CHECK_MESSAGE(story.find(std::string(step.what) + "; ticked when " + step.by) !=
                          std::string::npos,
                      story);
        // ...AND A MOVE NAMES WHAT IT PRESSES, never a key the weaver may have moved.
        CHECK_MESSAGE(std::string(step.move).find("ctrl+") == std::string::npos, step.move);
    }
}
