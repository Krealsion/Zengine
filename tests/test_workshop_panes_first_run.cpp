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

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace fr = zengine::first_run;

/// A STAND-IN FOR THE EDITOR SWITCH, in its office: it records each ask and answers it as a case
/// says -- the status with the choice it holds, a switch with what `on_switch` returns.
class SwitchStandIn
    : public loom::WeaveBase<SwitchStandIn, SeatState,
                             loom::Accept<EditorSwitchRequested, EditorSwitchConfirmed,
                                          EditorSwitchStatusRequested>,
                             loom::Emit<EditorSwitchAnswered>> {
public:
    std::string active = fr::kStandardChoice;
    std::vector<std::string> asked; ///< `request <destination>` or `confirm <op> <consent>`
    std::function<EditorSwitchAnswered(const EditorSwitchRequested&)> on_switch;

    void on(const EditorSwitchStatusRequested&, loom::Mail& mail) {
        EditorSwitchAnswered a;
        a.outcome = switch_outcome::kStatus;
        a.active = active;
        (void)mail.answer(a);
    }
    void on(const EditorSwitchRequested& r, loom::Mail& mail) {
        asked.push_back("request " + r.destination);
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

/// THE QUICK SETUP'S CENSUS: its three rows, in no mode.
const CensusDriverOf kFirstRunCensus{fr::kFirstRunRole, [] {
    OfficeCensus census;
    TempDir dir("first-run-census");
    PaneRig r;
    (void)start(r, dir.file("prefs.json"));
    const RuntimePane* held = r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane);
    REQUIRE(held != nullptr);
    census_take(census, fr::kFirstRunRole, *held);
    return census;
}};

/// THE QUICK SETUP'S PRESSES: each row on a quick setup made afresh.
const PressDriverOf kFirstRunPresses{fr::kFirstRunRole, [](PressWitness& w) {
    std::vector<v3::PaneActionRow> rows;
    {
        TempDir dir("first-run-rows");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        rows = r.session().panes.runtime.find(fr::kFirstRunRole, fr::kSetupPane)->actions;
    }
    for (const v3::PaneActionRow& row : rows) {
        TempDir dir("first-run-press");
        PaneRig r;
        (void)start(r, dir.file("prefs.json"));
        w.judge(r, fr::kSetupPane, "", row);
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
        act(r, fr::kActionDown); // the box, then Done
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

    // AND THE OFFICE IS ADMITTED WITH THE SWITCH'S FOUR ASKS, TO THE SWITCH'S OFFICE ALONE.
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
             {EditorSwitchStatusRequested::zen_name, EditorSwitchStatusRequested::zen_version}}) {
        CAPTURE(name);
        CHECK(v.grant.permits_role(name, version, kEditorSwitchRole));
        CHECK_FALSE(v.grant.permits_role(name, version, kWorkshopProvider));
    }
    // ...its pictures only to the windows, never to Workshop; and no load.
    CHECK_FALSE(v.grant.permits_role(zengine::graphics::Drawn::zen_name, zengine::graphics::Drawn::zen_version,
                                     kWorkshopProvider));
    CHECK(v.grant.permits_role(zengine::graphics::Drawn::zen_name, zengine::graphics::Drawn::zen_version,
                               std::string(zengine::graphics::kWindowOfficePrefix) + zengine::graphics::kSdlWindow));
    CHECK_FALSE(v.grant.permits_role(loom::LoadWeave::zen_name, loom::LoadWeave::zen_version,
                                     "zen.manager"));
}

TEST_CASE("WL-FIRST-02: the quick setup's lines name the box, the pages and Done and Skip, each "
          "press choosing its own item") {
    fr::Moment m;
    const std::vector<fr::Line> lines = fr::lines_of(m);
    std::vector<fr::Item> items;
    bool setups = false, started = false;
    for (const fr::Line& l : lines) {
        if (l.item != fr::Item::kNone) items.push_back(l.item);
        setups = setups || l.text.find(fr::kSetupsPage) != std::string::npos;
        started = started || l.text.find(fr::kGettingStartedPage) != std::string::npos;
    }
    CHECK(items == std::vector<fr::Item>{fr::Item::kNeovim, fr::Item::kDone, fr::Item::kSkip});
    CHECK(setups);
    CHECK(started);
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
    CHECK(fr::stepped(fr::Item::kNeovim, -1) == fr::Item::kNeovim);
    CHECK(fr::stepped(fr::Item::kSkip, 1) == fr::Item::kSkip);
    CHECK(fr::stepped(fr::Item::kNeovim, 1) == fr::Item::kDone);
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
        const ui::Rect body = external_body_rect(r.session(), kind);
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
