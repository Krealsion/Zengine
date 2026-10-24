// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- a guest that teaches: a Tutor reads the panes its `capture` doors name
// and marks the control it wants the weaver to press, and the weaver presses. Driven through the real
// `zengine-builder-pane` and desktop images over the real pane protocol, with the guest a participant
// holding exactly the grant its guests-file row compiles to (`guests::grant_for`), its doors Loom door
// rules Workshop asks about at each read and mark, on a weaver's host. Pictures are read as each window
// draws them: the terminal's raster and the window's plan.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"
#include "builder_rig.hpp"

#include "builder/builder-pane/vocabulary.hpp"
#include "graphics/light_paint.hpp"
#include "graphics/window/terminal_io.hpp"
#include "input/vocabulary.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"
#include "workshop/guests.hpp"
#include "workshop/guide_vocabulary.hpp"
#include "workshop/pane_document.hpp"

#include <zen/host/grant_wiring.hpp>
#include <zen/switchboard/door.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {

using builder_rig::BuilderRig;
using builder_rig::catalog_of;
namespace pane = zengine::builder_pane;
namespace desktop = zengine::desktop_pane;
namespace guests = zengine::workshop::guests;

struct TutorState {
    std::int64_t heard = 0;
    ZEN_SHAPE(TutorState, 1, ZEN_FIELD(heard));
};

/// A GUEST AS ITS SESSION STANDS ON THE BUS: it asks Workshop by message and keeps every answer,
/// and may try the sends its row does not grant, so a case sees the bus refuse them.
class TutorWeave
    : public loom::WeaveBase<TutorWeave, TutorState,
                             loom::Accept<SeatDo, GuideAnswered, PaneView, DeskRead,
                                          KeymapShown, PaneDocumentShown, loom::Refused>,
                             loom::Emit<GuideRequested, PaneViewRequested, DeskReadRequested,
                                        KeymapRequested, PaneDocumentRequested,
                                        input::InputSessionRequested, input::InjectInput>> {
public:
    std::function<void(TutorWeave&, loom::Mail&)> next;
    std::vector<GuideAnswered> guides;
    std::vector<PaneView> views;
    std::vector<DeskRead> desks;
    std::vector<KeymapShown> keys;
    std::vector<PaneDocumentShown> docs;
    std::vector<std::string> refusals;
    std::vector<bool> sent; ///< whether each send a case made was let onto the bus

    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = next;
            next = nullptr;
            what(*this, mail);
        }
    }
    void on(const GuideAnswered& a, loom::Mail&) { guides.push_back(a); }
    void on(const PaneView& v, loom::Mail&) { views.push_back(v); }
    void on(const DeskRead& d, loom::Mail&) { desks.push_back(d); }
    void on(const KeymapShown& k, loom::Mail&) { keys.push_back(k); }
    void on(const PaneDocumentShown& d, loom::Mail&) { docs.push_back(d); }
    void on(const loom::Refused& r, loom::Mail&) { refusals.push_back(r.reason); }
};

/// ONE GUEST: its row as its guests file of version 3 says it, its participant registered with
/// exactly the grant that row compiles to, and the host's seam answering it as that row.
struct Tutor {
    guests::GuestRow row;
    TutorWeave* weave = nullptr;
    loom::WeaveId id{};

    void act(loom::Switchboard& bus, std::function<void(TutorWeave&, loom::Mail&)> what) {
        weave->next = std::move(what);
        (void)bus.send(id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        bus.drain_until_idle();
    }
};

guests::GuestRow tutor_row(std::string name, std::vector<std::string> capture,
                           std::vector<std::string> guide,
                           std::vector<std::string> may = {guests::kPowerCapture, guests::kPowerGuide}) {
    guests::GuestRow row;
    row.name = std::move(name);
    row.credential = row.name + "-cred";
    row.role = "tutor";
    row.may = std::move(may);
    row.by_door = true;
    row.capture = std::move(capture);
    row.guide = std::move(guide);
    return row;
}

/// THE BUILDER ON A WEAVER'S DESK, with the shipped desktop beside it (its Pane Manager and
/// Hotkeys), and the guests a case admits.
struct TutorDesk {
    BuilderRig b;
    guests::GuestsFile file;
    std::vector<std::unique_ptr<Tutor>> tutors;

    explicit TutorDesk(const char* tag, bool with_editor = false) : b(tag) {
        file.version = guests::kGuestsFileVersion;
        b.r.host.host_fact.development = false;
        b.r.host.input_authority = [&bus = b.r.bus](loom::WeaveId who) {
            return bus.alive(who) ? loom::host_describe_only_authority(bus, who) : loom::GrantAuthority{};
        };
        b.r.host.guest_row = [this](loom::WeaveId who) {
            for (const auto& t : tutors) {
                if (t->id == who) return guests::facts_of(t->row, file);
            }
            return scope::GuestRowFacts{};
        };
        b.tool->catalog = catalog_of({{"tally", "tally"}});
        // A MARK IS A ROLE'S INK, so the desk wears a skin a terminal draws roles in: `block`.
        prefs_persist::Prefs prefs;
        prefs_persist::put_office_value(prefs, kWorkshopProvider, "workshop.skin",
                                        PaneSetting{"workshop.skin", std::nullopt, std::nullopt,
                                                    std::string("block")});
        b.r.host.prefs_path = (b.root / "workshop-prefs.json").generic_string();
        REQUIRE(prefs_persist::save_file(b.r.host.prefs_path, prefs).accepted);
        b.open(160, 48, with_editor, true, true, false, /*with_desktop=*/true);
    }

    /// THE SAME DESK ON THE OS WINDOW.
    void on_window() { b.r.extent_on_window(160, 48); }

    Tutor& admit(guests::GuestRow row) {
        auto t = std::make_unique<Tutor>();
        t->row = std::move(row);
        file.rows.push_back(t->row);
        auto held = std::make_unique<TutorWeave>();
        t->weave = held.get();
        t->id = b.r.bus.register_weave(std::move(held), guests::grant_for(t->row));
        t->weave->zen_set_self(t->id);
        tutors.push_back(std::move(t));
        return *tutors.back();
    }

    GuideAnswered mark(Tutor& t, const std::string& pane_ref, const std::string& control,
                       const std::string& words = "build this one") {
        const std::size_t before = t.weave->guides.size();
        t.act(b.r.bus, [&](TutorWeave& w, loom::Mail& mail) {
            w.sent.push_back(mail.send_to_role(kWorkshopProvider,
                                               GuideRequested{pane_ref, control, words}).valid());
        });
        REQUIRE(t.weave->guides.size() == before + 1);
        return t.weave->guides.back();
    }

    std::string read_refused(Tutor& t, const std::string& office, const std::string& pane_key) {
        const std::size_t views = t.weave->views.size(), refused = t.weave->refusals.size();
        t.act(b.r.bus, [&](TutorWeave&, loom::Mail& mail) {
            (void)mail.send_to_role(kWorkshopProvider, PaneViewRequested{office, pane_key, 0, {}});
        });
        if (t.weave->views.size() > views) return {};
        REQUIRE(t.weave->refusals.size() == refused + 1);
        return t.weave->refusals.back();
    }

    PaneView read(Tutor& t, const std::string& office, const std::string& pane_key) {
        const std::size_t views = t.weave->views.size();
        t.act(b.r.bus, [&](TutorWeave&, loom::Mail& mail) {
            (void)mail.send_to_role(kWorkshopProvider, PaneViewRequested{office, pane_key, 0, {}});
        });
        REQUIRE_MESSAGE(t.weave->views.size() == views + 1,
                        (t.weave->refusals.empty() ? std::string() : t.weave->refusals.back()));
        return t.weave->views.back();
    }

    DeskRead desk(Tutor& t) {
        const std::size_t before = t.weave->desks.size();
        b.r.clock.now += 1000; // past the desk read's rate
        t.act(b.r.bus, [](TutorWeave&, loom::Mail& mail) {
            (void)mail.send_to_role(kWorkshopProvider, DeskReadRequested{});
        });
        REQUIRE(t.weave->desks.size() == before + 1);
        return t.weave->desks.back();
    }

    /// THE WEAVER'S OWN PRESS AND RELEASE at the point a part's reading names, in its medium's
    /// space: what the platform reports, attributed to no guest.
    void weaver_press(const PanePart& part) {
        // ...as the window that shows the desk reads the weaver's hand from its platform.
        for (const bool down : {true, false}) {
            b.r.hand_button(part.x, part.y, part.space, down);
        }
    }
};

const PanePart* part_named(const PaneView& view, const std::string& name) {
    for (const PanePart& p : view.parts) {
        if (p.name == name) return &p;
    }
    return nullptr;
}

namespace rw = zengine::testing::rig_windows;

/// THE CELLS A TERMINAL WINDOW SHOWS IN THE GUIDE ROLE'S INK: what a guest's marks drew there.
std::vector<rw::Cell> guide_cells(TutorDesk& d) {
    std::vector<rw::Cell> out;
    for (const rw::Cell& c : d.b.r.windows.cells()) {
        if (c.role == gfx::role::kGuide) out.push_back(c);
    }
    return out;
}

/// THE QUADS THE OS WINDOW DRAWS IN THE GUIDE ROLE'S INK: a mark's outline there.
std::vector<rw::Quad> guide_quads(TutorDesk& d) {
    std::vector<rw::Quad> out;
    for (const rw::Quad& q : d.b.r.windows.quads()) {
        if (q.role == gfx::role::kGuide) out.push_back(q);
    }
    return out;
}

/// WHETHER ANY MARK IS DRAWN, in either window: a cell or a quad in the guide role's ink.
bool mark_drawn(TutorDesk& d) {
    return d.b.r.windows.window() == gfx::kSdlWindow ? !guide_quads(d).empty()
                                                       : !guide_cells(d).empty();
}

/// THE OFFICE THAT HOLDS INPUT, as a case needs one: it counts what reaches it.
class InputOffice
    : public loom::WeaveBase<InputOffice, TutorState,
                             loom::Accept<input::InputSessionRequested, input::InjectInput>, loom::Emit<>> {
public:
    explicit InputOffice(std::int64_t& heard) : heard_(&heard) {}
    void on(const input::InputSessionRequested&, loom::Mail&) { ++*heard_; }
    void on(const input::InjectInput&, loom::Mail&) { ++*heard_; }

private:
    std::int64_t* heard_;
};

bool says(const std::string& text, const std::string& words) { return text.find(words) != std::string::npos; }

/// A BUS OBSERVER, removed when the case leaves its scope.
struct Watching {
    loom::Switchboard& bus;
    loom::ObserverId id;
    ~Watching() { bus.remove_observer(id); }
};

/// THE WEAVER OPENS ONE FILE IN THE EDITOR, through the Editor's own door, as Files' Return does.
SourceOpened open_in_editor(PaneRig& r, const std::string& path) {
    auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
    DoorAsker* asker = held.get();
    loom::Grant grant;
    grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
    asker->id = r.bus.register_weave(std::move(held), std::move(grant), std::string(kDoorAskerOffice));
    asker->zen_set_self(asker->id);
    asker->next = [path](DoorAsker& a, loom::Mail& mail) { a.ask(mail, kEditorRole, OpenSourceRequested{path}); };
    (void)r.bus.send(asker->id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
    REQUIRE(asker->opens.size() == 1);
    const SourceOpened said = asker->opens.back();
    r.bus.unregister_weave(asker->id);
    return said;
}

std::string builder_door() { return std::string(pane::kBuilderPaneRole) + "/" + pane::kBuilderPane; }
std::string launcher_door() { return std::string(kDesktopRole) + "/" + desktop::kLauncherPane; }

} // namespace

// ============================================================================
// THE WALK: read, mark, the weaver presses, the desk moves, the next mark
// ============================================================================

TEST_CASE("the Tutor's walk on a weaver's host, in a terminal and in a window: it reads the Builder, marks [build], the weaver's press inside the mark ends it and builds, the desk moves, and the next mark stands") {
    bool window = false;
    SUBCASE("in a terminal") { window = false; }
    SUBCASE("in a window") { window = true; }
    CAPTURE(window);
    TutorDesk d("guide-walk");
    if (window) d.on_window();
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*", launcher_door()},
                                     {"zengine.builder-pane/" "*", launcher_door()}));
    // READ: the Builder, its Build control drawn and usable.
    const PaneView before = d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane);
    const PanePart* build = part_named(before, "control:" + std::string(pane::kActionBuild));
    REQUIRE_MESSAGE(build != nullptr, d.b.text());
    REQUIRE(build->available);
    CHECK(build->space == (window ? input::space::kPixels : input::space::kCells));
    const DeskRead first = d.desk(tutor);
    // MARK: by the address a press names, the guest named in the answer.
    const GuideAnswered marked = d.mark(tutor, builder_door(), pane::kActionBuild);
    REQUIRE_MESSAGE(marked.shown, marked.said);
    CHECK(says(marked.said, "marked control:builder.build in zengine.builder-pane/builder for the weaver, as guest 'tutor'"));
    if (window) CHECK(guide_quads(d).size() == 4);
    else CHECK_FALSE(guide_cells(d).empty());
    // THE WEAVER PRESSES INSIDE IT, at the point its reading names: the mark ends, its guest is
    // told, and the press builds.
    const std::size_t builds = d.b.tool->asked.size();
    d.weaver_press(*build);
    REQUIRE_FALSE(tutor.weave->guides.empty());
    CHECK(says(tutor.weave->guides.back().said, "ended: the weaver pressed it"));
    CHECK_FALSE(tutor.weave->guides.back().shown);
    CHECK_FALSE(mark_drawn(d));
    CHECK(d.b.tool->asked.size() == builds + 1);
    // THE DESK MOVES: the Builder's stamp is another picture's.
    d.b.tool_says();
    const DeskRead after = d.desk(tutor);
    const auto stamp_of_builder = [](const DeskRead& r) {
        for (const PaneStamp& s : r.stamps) {
            if (s.provider == pane::kBuilderPaneRole) return s.fingerprint;
        }
        return std::int64_t{0};
    };
    CHECK(stamp_of_builder(first) != 0);
    CHECK(stamp_of_builder(after) != stamp_of_builder(first));
    // THE NEXT MARK STANDS.
    const GuideAnswered next = d.mark(tutor, builder_door(), pane::kActionOutput, "read what it said");
    CHECK_MESSAGE(next.shown, next.said);
}

// ============================================================================
// THE MARK'S FORM, as each window draws it
// ============================================================================

TEST_CASE("a mark in a terminal is its part's own cells in the guide role: every row of the pane reads as before, and its label stands on a body row wholly free or is not drawn, the guest told which") {
    TutorDesk d("guide-terminal");
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    const std::vector<std::string> plain = d.b.r.shown().rows;
    const GuideAnswered marked = d.mark(tutor, builder_door(), pane::kActionBuild);
    REQUIRE_MESSAGE(marked.shown, marked.said);
    const std::vector<std::string> shown = d.b.r.shown().rows;
    const std::vector<rw::Cell> inked = guide_cells(d);
    // ITS INK IS A COLOUR AND NO ATTRIBUTE, as every role's is, so the cells after it are drawn in
    // their own ink, neither reversed nor bold.
    const std::string ink = gfx::window::sgr_for_role(static_cast<int>(gfx::role::kGuide));
    REQUIRE(ink.rfind("\x1b[", 0) == 0);
    const std::string params = ink.substr(2, ink.size() - 3);
    for (std::size_t at = 0; at <= params.size();) {
        const std::size_t end = std::min(params.find(';', at), params.size());
        const int code = std::stoi(params.substr(at, end - at));
        CAPTURE(code);
        CHECK(((code >= 30 && code <= 39) || (code >= 90 && code <= 97)));
        at = end + 1;
    }
    // THE PART'S OWN CELLS, in the guide role, saying what they said.
    const PaneView view = d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane);
    const PanePart* build = part_named(view, "control:" + std::string(pane::kActionBuild));
    REQUIRE(build != nullptr);
    const std::int64_t part_col = gfx::cell_of_pixel(build->place.x);
    const std::int64_t part_row = gfx::cell_of_pixel(build->place.y);
    std::string on_part;
    for (const rw::Cell& cell : inked) {
        if (cell.row == part_row && cell.col >= part_col &&
            cell.col < part_col + static_cast<std::int64_t>(build->text.size())) {
            on_part += cell.glyph;
        }
    }
    CHECK(on_part == build->text);
    // NO OUTLINE CELL: a terminal mark draws no rectangle -- the guide's ink stands on the part's
    // own cells and on the label's row, and nowhere else.
    for (const rw::Cell& cell : inked) {
        CAPTURE(cell.col);
        CAPTURE(cell.row);
        const bool on_the_part = cell.row == part_row && cell.col >= part_col &&
                                 cell.col < part_col + static_cast<std::int64_t>(build->text.size());
        const auto row = static_cast<std::size_t>(cell.row);
        const bool on_the_label = row < shown.size() && row < plain.size() && shown[row] != plain[row];
        CHECK((on_the_part || on_the_label));
    }
    // EVERY ROW READS AS BEFORE, but a label's own row, which was wholly blank inside the body.
    REQUIRE(shown.size() == plain.size());
    const bool labelled = says(marked.said, "its label stands beside it");
    CHECK((labelled || says(marked.said, "its label is not drawn: no free row of the pane's body beside it")));
    std::size_t changed = 0;
    for (std::size_t row = 0; row < shown.size(); ++row) {
        if (shown[row] == plain[row]) continue;
        ++changed;
        CAPTURE(row);
        CHECK(says(shown[row], "tutor: \"build this one\""));
        const ui::Rect body = external_body_rect(d.b.r.session(), d.b.kind);
        const std::string before = plain[row].substr(static_cast<std::size_t>(body.x),
                                                     static_cast<std::size_t>(body.w));
        CHECK(before.find_first_not_of(' ') == std::string::npos);
    }
    CHECK(changed == (labelled ? 1u : 0u));
    // A PART WITH WORDS ON THE ROWS ABOVE AND BELOW IT, the chosen recipe's: no free row, so no
    // label, said; and every row of the pane still reads as it did.
    const GuideAnswered crowded = d.mark(tutor, builder_door(), "part:recipe:tally");
    REQUIRE_MESSAGE(crowded.shown, crowded.said);
    CHECK_MESSAGE(says(crowded.said, "its label is not drawn: no free row of the pane's body beside it"), crowded.said);
    const std::vector<std::string> crowded_rows = d.b.r.shown().rows;
    REQUIRE(crowded_rows.size() == plain.size());
    for (std::size_t row = 0; row < plain.size(); ++row) {
        CAPTURE(row);
        CAPTURE(crowded_rows[row]);
        CHECK(crowded_rows[row] == plain[row]);
    }
}

TEST_CASE("a mark in a window is an outline one pixel wide around its part inside the pane's body, in the guide role's ink as the window's plan draws it, and its label names the guest") {
    TutorDesk d("guide-window");
    d.on_window();
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    const PaneView view = d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane);
    const PanePart* build = part_named(view, "control:" + std::string(pane::kActionBuild));
    REQUIRE(build != nullptr);
    const GuideAnswered marked = d.mark(tutor, builder_door(), pane::kActionBuild);
    REQUIRE_MESSAGE(marked.shown, marked.said);
    const std::vector<rw::Quad> edges = guide_quads(d);
    REQUIRE(edges.size() == 4);
    const DeskRect& r = build->place;
    for (const rw::Quad& e : edges) {
        CHECK((e.w == 1 || e.h == 1));
        // AROUND THE PART, never over it.
        const bool over = e.x < r.x + r.w && r.x < e.x + e.w && e.y < r.y + r.h && r.y < e.y + e.h;
        CHECK_FALSE(over);
    }
    // THE WINDOW DRAWS THEM IN THE GUIDE ROLE'S INK, which is no other role's in its look.
    const gfx::light::Look look = gfx::light::night_look();
    for (const std::int64_t other : {gfx::role::kFill, gfx::role::kAccent, gfx::role::kMuted,
                                     gfx::role::kAlert}) {
        CHECK(look.colour.at(gfx::role::kGuide) != look.colour.at(other));
    }
    const bool named = says(d.b.r.shown().text(), "tutor: \"build this one\"");
    CHECK(named == says(marked.said, "its label stands beside it"));
}

TEST_CASE("a mark's part reads the same with the mark as without it, and a mark is no cover") {
    TutorDesk d("guide-reading");
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    const PaneView before = d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane);
    REQUIRE(d.mark(tutor, builder_door(), pane::kActionBuild).shown);
    const PaneView after = d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane);
    CHECK(after.stamp.fingerprint == before.stamp.fingerprint);
    CHECK(after.covered.by.empty());
    REQUIRE(after.parts.size() == before.parts.size());
    for (std::size_t i = 0; i < after.parts.size(); ++i) {
        CHECK(after.parts[i].name == before.parts[i].name);
        CHECK(after.parts[i].text == before.parts[i].text);
        CHECK(after.parts[i].available == before.parts[i].available);
    }
    REQUIRE(after.words.size() == before.words.size());
    for (std::size_t i = 0; i < after.words.size(); ++i) CHECK(after.words[i].text == before.words[i].text);
}

// ============================================================================
// HOW A MARK ENDS
// ============================================================================

TEST_CASE("a mark ends, and never comes back, when its part leaves a settled picture, and its guest is told why") {
    TutorDesk d("guide-ends");
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    REQUIRE(d.mark(tutor, builder_door(), pane::kActionBuild).shown);
    // THE WEAVER OPENS THE RECIPE LIST: the Build control is no longer drawn.
    d.b.r.key(input::scan::kReturn);
    REQUIRE(part_named(d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane),
                       "control:" + std::string(pane::kActionBuild)) == nullptr);
    REQUIRE_FALSE(tutor.weave->guides.empty());
    CHECK_MESSAGE(says(tutor.weave->guides.back().said, "ended: its part control:builder.build left the pane's picture"),
                  tutor.weave->guides.back().said);
    CHECK_FALSE(mark_drawn(d));
    // ...AND CLOSES IT: the control is back, and the mark is not.
    d.b.r.key(input::scan::kEscape);
    const PaneView back = d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane);
    REQUIRE(part_named(back, "control:" + std::string(pane::kActionBuild)) != nullptr);
    CHECK_FALSE(mark_drawn(d));
    // A NEWER MARK REPLACES THE SESSION'S ONE, and an empty control clears it.
    REQUIRE(d.mark(tutor, builder_door(), pane::kActionBuild).shown);
    REQUIRE(d.mark(tutor, builder_door(), pane::kActionOutput, "the output").shown);
    CHECK_FALSE(says(d.b.r.shown().text(), "build this one"));
    const GuideAnswered cleared = d.mark(tutor, builder_door(), "");
    CHECK(cleared.said == "cleared");
    CHECK_FALSE(mark_drawn(d));
}

TEST_CASE("a mark ends when its guest's session ends, and a guest's own press refused at the bus never ends one") {
    TutorDesk d("guide-session");
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    REQUIRE(d.mark(tutor, builder_door(), pane::kActionBuild).shown);
    // THE TUTOR'S OWN PRESS: its row holds no input, so the bus refuses the session and the press
    // before the office that holds input hears either.
    std::int64_t heard = 0;
    {
        auto office = std::make_unique<InputOffice>(heard);
        InputOffice* raw = office.get();
        raw->zen_set_self(d.b.r.bus.register_weave(std::move(office), loom::emit_default_grant(*raw),
                                                   std::string(input::kInputRole)));
    }
    std::size_t refused = 0;
    Watching watching{d.b.r.bus, d.b.r.bus.add_observer([&](const loom::BusEvent& seen) {
        if (seen.kind == loom::EventKind::Refused && seen.sender == tutor.id) ++refused;
    })};
    tutor.act(d.b.r.bus, [](TutorWeave&, loom::Mail& mail) {
        (void)mail.send_to_role(input::kInputRole, input::InputSessionRequested{"tutor"});
        (void)mail.send_to_role(input::kInputRole, input::InjectInput{1, {}});
    });
    CHECK(refused == 2);
    CHECK(heard == 0);
    CHECK(mark_drawn(d));
    // ITS SESSION ENDS: the next picture draws no mark of it.
    d.b.r.bus.unregister_weave(tutor.id);
    d.b.regrant();
    CHECK_FALSE(mark_drawn(d));
}

// ============================================================================
// WHERE NO MARK STANDS
// ============================================================================

TEST_CASE("a second guest's mark on a marked pane is refused, and no mark stands on Connections, the Hotkeys pane or a part whose press opens either") {
    TutorDesk d("guide-refusals");
    Tutor& one = d.admit(tutor_row("one", {"*"}, {"*"}));
    Tutor& two = d.admit(tutor_row("two", {"*"}, {"*"}));
    REQUIRE(d.mark(one, builder_door(), pane::kActionBuild).shown);
    const GuideAnswered second = d.mark(two, builder_door(), pane::kActionOutput);
    CHECK_FALSE(second.shown);
    CHECK(says(second.said, "refused: guest 'one' marks that pane already; one guest mark a pane"));
    // THE PANES ONLY THE WEAVER'S HAND READS, whatever shows there.
    const GuideAnswered hotkeys = d.mark(two, std::string(kDesktopRole) + "/" + kHotkeysPane, "hotkeys.commit");
    CHECK_FALSE(hotkeys.shown);
    CHECK(says(hotkeys.said, "the Hotkeys pane edits the weaver's keymap"));
    const GuideAnswered connections = d.mark(two, std::string(kConnectionsRole) + "/connections", "any");
    CHECK_FALSE(connections.shown);
    CHECK(says(connections.said, "Connections says who is connected"));
    // ...AND THE PARTS THAT OPEN THEM: the Pane Manager's rows and the desktop's own key.
    d.b.r.key(input::scan::kP, input::mod::kCtrl);
    for (const std::string& row : {std::string("part:pane:zengine.desktop/hotkeys"),
                                   std::string("part:pane:zengine.connections/connections")}) {
        CAPTURE(row);
        const GuideAnswered opening = d.mark(two, launcher_door(), row);
        CHECK_FALSE(opening.shown);
        CHECK(says(opening.said, "opens a pane only the weaver's hand reaches"));
    }
    const GuideAnswered key = d.mark(two, kDesktopRole, desktop::kActionHotkeys);
    CHECK_FALSE(key.shown);
    CHECK(says(key.said, "opens a pane only the weaver's hand reaches"));
}

TEST_CASE("a mark beyond the row's guide doors is refused in the same words whether its part shows or not, and a row without guide is refused at the bus") {
    TutorDesk d("guide-doors");
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*", launcher_door()}, {launcher_door()}));
    const GuideAnswered shown_part = d.mark(tutor, builder_door(), pane::kActionBuild);
    const GuideAnswered no_part = d.mark(tutor, builder_door(), "builder.no-such-control");
    CHECK_FALSE(shown_part.shown);
    CHECK(shown_part.said == no_part.said);
    CHECK(says(shown_part.said, "is outside guest 'tutor''s `guide` doors"));
    CHECK_FALSE(mark_drawn(d));
    // A ROW THAT MAY NOT GUIDE: its ask never reaches Workshop.
    Tutor& reader = d.admit(tutor_row("reader", {"*"}, {}, {guests::kPowerCapture}));
    std::size_t delivered = 0;
    Watching watching{d.b.r.bus, d.b.r.bus.add_observer([&](const loom::BusEvent& seen) {
        if (seen.kind == loom::EventKind::Delivered && seen.schema_name == GuideRequested::zen_name) ++delivered;
    })};
    reader.act(d.b.r.bus, [](TutorWeave& w, loom::Mail& mail) {
        w.sent.push_back(mail.send_to_role(kWorkshopProvider,
                                           GuideRequested{"zengine.builder-pane/builder", "builder.build", "x"}).valid());
    });
    CHECK(delivered == 0);
    CHECK(reader.weave->guides.empty());
}

TEST_CASE("a control drawn as no part is answered not shown, with the weaver's key for it") {
    TutorDesk d("guide-keys");
    Tutor& tutor = d.admit(tutor_row("tutor", {"*"}, {"*"}));
    // A ROW ABOVE EVERY PANE: a key, wherever the weaver stands.
    const GuideAnswered app = d.mark(tutor, kDesktopRole, desktop::kActionPanes);
    CHECK_FALSE(app.shown);
    CHECK(says(app.said, "is a key, with no part to mark; the weaver's key is "));
    // A PANE'S ROW THE PANE DRAWS NO CONTROL FOR.
    const GuideAnswered realize = d.mark(tutor, builder_door(), pane::kActionBuildRealize);
    CHECK_FALSE(realize.shown);
    CHECK(says(realize.said, "is in force, and the weaver's key is "));
}

// ============================================================================
// READING BY PANE
// ============================================================================

TEST_CASE("a row that reads by pane reads the Builder's words and the Editor's place, never the Editor's words, and hears the keymap of its own panes and the rows above every pane") {
    TutorDesk d("guide-reads", /*with_editor=*/true);
    d.b.r.pick(PaneRef{"zengine.editor", "editor"});
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    // ITS OWN PANE, read whole.
    CHECK_FALSE(d.read(tutor, pane::kBuilderPaneRole, pane::kBuilderPane).parts.empty());
    // THE EDITOR'S PAGE, refused without a word of it.
    const std::string refused = d.read_refused(tutor, "zengine.editor", "editor");
    CHECK(says(refused, "pane view withheld: pane zengine.editor/editor is outside guest 'tutor''s `capture` doors"));
    // THE DESK: the Editor said as its place, name and holder; no fingerprint, no reading, and no
    // band or desk number, which move with every pane's words.
    const DeskRead desk = d.desk(tutor);
    bool editor_placed = false;
    for (const DeskPane& p : desk.desk.panes) editor_placed = editor_placed || p.provider == "zengine.editor";
    CHECK(editor_placed);
    for (const PaneStamp& s : desk.stamps) {
        if (s.provider != "zengine.editor") continue;
        CHECK(s.fingerprint == 0);
        CHECK(s.incarnation == 0);
        CHECK(s.holder != 0);
    }
    for (const PaneView& v : desk.panes) CHECK(v.stamp.provider != "zengine.editor");
    CHECK(desk.desk.words.empty());
    CHECK(desk.desk.desk == 0);
    // THE KEYMAP: the Builder's rows and the rows above every pane, and none of the Editor's.
    tutor.act(d.b.r.bus, [](TutorWeave&, loom::Mail& mail) {
        (void)mail.send_to_role(kWorkshopProvider, KeymapRequested{});
    });
    REQUIRE_FALSE(tutor.weave->keys.empty());
    bool builders = false, above = false;
    for (const ShownBinding& row : tutor.weave->keys.back().rows) {
        CHECK(row.office != "zengine.editor");
        builders = builders || row.office == pane::kBuilderPaneRole;
        above = above || (row.office == kDesktopRole && row.pane.empty());
    }
    CHECK(builders);
    CHECK(above);
    for (const ShownModes& m : tutor.weave->keys.back().modes) CHECK(m.office != "zengine.editor");
    // ITS DOORS ARE LOOM'S: widened by a rule the row did not have, the Editor reads.
    Tutor& wider = d.admit(tutor_row("wider", {"zengine.builder-pane/" "*", "zengine.editor/editor"},
                                     {"zengine.builder-pane/" "*"}));
    CHECK(d.read_refused(wider, "zengine.editor", "editor").empty());
}

TEST_CASE("every guest is refused the words of the Hotkeys pane and of an editor showing the guests file, whatever it reads") {
    TutorDesk d("guide-weaver-only", /*with_editor=*/true);
    Tutor& whole = d.admit(tutor_row("whole", {"*"}, {"*"}));
    d.b.r.key(input::scan::kK, input::mod::kCtrl); // the weaver opens the Hotkeys pane
    const std::string hotkeys = d.read_refused(whole, kDesktopRole, kHotkeysPane);
    CHECK(says(hotkeys, "is read only by the weaver's hand: the Hotkeys pane edits the weaver's keymap"));
    // THE GUESTS FILE, opened in the Editor by the weaver.
    const std::filesystem::path guests_file = d.b.root / "guests.json";
    std::ofstream(guests_file) << R"({"version":"3","guests":[]})";
    d.b.r.host.host_fact.guests_file = std::filesystem::absolute(guests_file).lexically_normal().string();
    REQUIRE(open_in_editor(d.b.r, guests_file.generic_string()).accepted);
    const std::string editor = d.read_refused(whole, "zengine.editor", "editor");
    CHECK(says(editor, "shows the guests file, which only the weaver's hand reads"));
    // ANOTHER FILE IN THE SAME EDITOR reads.
    const std::filesystem::path notes = d.b.root / "notes.txt";
    std::ofstream(notes) << "alpha\n";
    REQUIRE(open_in_editor(d.b.r, notes.generic_string()).accepted);
    CHECK(d.read_refused(whole, "zengine.editor", "editor").empty());
}

TEST_CASE("a pane's manual is answered to a guest under its capture doors, and another office's manual is refused at the bus") {
    TutorDesk d("guide-manual");
    Tutor& tutor = d.admit(tutor_row("tutor", {"zengine.builder-pane/" "*"}, {"zengine.builder-pane/" "*"}));
    tutor.act(d.b.r.bus, [](TutorWeave&, loom::Mail& mail) {
        (void)mail.send_to_role(pane::kBuilderPaneRole, PaneDocumentRequested{pane::kBuilderPane, ""});
    });
    REQUIRE_FALSE(tutor.weave->docs.empty());
    CHECK(tutor.weave->docs.back().refusal.empty());
    CHECK_FALSE(tutor.weave->docs.back().sections.empty());
    // THE DESKTOP'S MANUAL: no door of the row names that office.
    const std::size_t docs = tutor.weave->docs.size();
    tutor.act(d.b.r.bus, [](TutorWeave& w, loom::Mail& mail) {
        w.sent.push_back(mail.send_to_role(kDesktopRole, PaneDocumentRequested{"launcher", ""}).valid());
    });
    CHECK(tutor.weave->docs.size() == docs);
}
