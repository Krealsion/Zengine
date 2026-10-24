// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// Suite `workshop_editor_switch` -- switching `zengine.editor` between authored choices in a LIVE
// Workshop, with no Neovim at all: the standard Editor's own image and copies of it built under
// other stems (`zengine-editor-pane-b`), three of which fail on purpose at exactly one step
// (`-silent`, `-refusing`, `-not-live`). What crosses, what the desk shows, who holds the office,
// what realization records, and every way a switch ends -- through the real coordinator, the real
// prepared replacement and the real handoff conversation.

#include "doctest.h"

#include "workshop_switch_rig.hpp"

#include "office_manual.hpp"
#include "pane_window.hpp"

#include "manuals/zengine.editor-switch.hpp"

namespace {

inline std::vector<load::ChoiceIntent> standard_and_twin() {
    return {load::ChoiceIntent{pane::kEditorPaneRole, "standard", pane::kEditorPaneStem},
            load::ChoiceIntent{pane::kEditorPaneRole, "twin", "zengine-editor-pane-b"}};
}

} // namespace

TEST_SUITE("workshop_editor_switch") {

TEST_CASE("a switch between two authored Editors carries the document, its unsaved edits and its caret, and moves the office") {
    SUBCASE("the successor asks for the graphics window's room once it holds the office, and is "
            "drawn the carried document there whole") {
        namespace gfx = zengine::graphics;
        namespace pw = zengine::testing::pane_window;
        const std::string area = pane_area(pane::kEditorPaneRole, pane::kEditorPane);
        struct Asked {
            loom::WeaveId sender;
            loom::EventKind kind;
        };
        std::vector<Asked> asked; // outlives the rig, whose bus the tap is on
        SwitchRig w("switch-carry-window");
        const loom::ObserverId tap = w.r.bus.add_observer([&](const loom::BusEvent& ev) {
            if (ev.schema_name == gfx::RoomAsked::zen_name &&
                ev.authored_role == pane::kEditorPaneRole &&
                (ev.kind == loom::EventKind::Delivered || ev.kind == loom::EventKind::Refused)) {
                asked.push_back(Asked{ev.sender, ev.kind});
            }
        });
        RoomAsks directory(w.r.bus);
        w.open(standard_and_twin());
        const loom::WeaveId first = w.holder();
        pw::Window window(w.r.bus);
        window.room(area, pane::kEditorPaneRole, 40, 10);
        (void)w.open_file("carry.cpp", "int one;\nint two;\n");
        // THE CARET PLACED BY A PRESS IN THE WINDOW THAT SHOWS THE EDITOR, the desk told of it as
        // the area's arranger, so the keys move there too.
        const std::int64_t above = w.read("notice").empty() ? 1 : 2;
        window.click(area, 4, above + 1, 1, kWorkshopProvider);
        w.r.bus.drain_until_idle();
        w.type("X");
        const EditorSwitchAnswered away = w.switch_to("twin");
        REQUIRE_MESSAGE(away.outcome == switch_outcome::kSwitched, away.detail);
        const loom::WeaveId second = w.holder();
        REQUIRE_FALSE(second == first);
        // EACH HOLDER ASKED ONCE, AS THE OFFICE IT HELD: the successor, once its admission had
        // activated it, and never while it was sealed.
        REQUIRE(asked.size() == 2);
        CHECK(asked[0].sender == first);
        CHECK(asked[1].sender == second);
        CHECK(asked[1].kind == loom::EventKind::Delivered);
        CHECK(directory.times(area, pane::kEditorPaneRole) == 2);
        // THE WINDOW, TOLD THE ROOM AGAIN as the directory passes the ask on, is drawn the carried
        // document whole, by the successor.
        const std::size_t mark = window.weave().heard.size();
        window.room(area, pane::kEditorPaneRole, 40, 10);
        REQUIRE(window.weave().heard.size() == mark + 1);
        CHECK(window.weave().heard.back().whole);
        const std::vector<std::string> rows = window.rows(area);
        CHECK(std::find(rows.begin(), rows.end(), "int Xtwo;") != rows.end());
        CHECK(window.weave().refusals.empty());
        CHECK(window.weave().strangers == 0);
        w.r.bus.remove_observer(tap);
    }
    SwitchRig s("switch-carry");
    s.open(standard_and_twin());
    const loom::WeaveId first = s.holder();
    const std::string path = s.open_file("carry.cpp", "int one;\nint two;\n");
    s.press_doc(1, 4);
    s.type("X");
    REQUIRE(s.status().rfind("UNSAVED", 0) == 0);
    REQUIRE(s.seat() != nullptr);
    REQUIRE(shows_canvas(*s.seat()));
    const std::int64_t incumbent_generation = std::stoll(s.read("doc_epoch"));

    const EditorSwitchAnswered away = s.switch_to("twin");
    CHECK_MESSAGE(away.outcome == switch_outcome::kSwitched, away.detail);
    CHECK(away.active == "twin");
    CHECK(away.destination == "twin");
    CHECK(away.op > 0);
    REQUIRE(away.choices.size() == 2);
    // THE OFFICE MOVED TO ANOTHER WEAVE, LOADED FROM THE OTHER CHOICE'S ARTIFACT, and the retired
    // one is gone.
    const loom::WeaveId second = s.holder();
    CHECK(second.valid());
    CHECK_FALSE(second == first);
    CHECK(s.r.kernel.is_loaded("zengine-editor-pane-b"));
    CHECK_FALSE(s.r.kernel.is_loaded(pane::kEditorPaneStem));
    CHECK(s.r.plan_->choice_holder(pane::kEditorPaneRole) == "zengine-editor-pane-b");
    // THE DOCUMENT CROSSED EXACTLY: the unsaved bytes, the saved copy, the caret.
    CHECK(s.read("path") == path);
    CHECK(s.read("text") == "int one;\nint Xtwo;\n");
    CHECK(s.read("saved_text") == "int one;\nint two;\n");
    CHECK(s.read("caret_row") == "1");
    CHECK(s.read("caret_byte") == "5");
    // THE DESK SHOWS THE SUCCESSOR IN THE SAME SEAT, drawing in the office's area; and the document
    // is a generation past the incumbent's.
    CHECK(s.r.session().panes.has(s.kind));
    CHECK(s.shows("UNSAVED"));
    CHECK(s.shows("int Xtwo;"));
    REQUIRE(s.seat() != nullptr);
    CHECK(shows_canvas(*s.seat()));
    const std::int64_t successor_generation = std::stoll(s.read("doc_epoch"));
    CHECK(successor_generation > incumbent_generation);

    // AND BACK, through the same law, to the plan's own row.
    const EditorSwitchAnswered back = s.switch_to("standard");
    CHECK_MESSAGE(back.outcome == switch_outcome::kSwitched, back.detail);
    REQUIRE(s.seat() != nullptr);
    CHECK(shows_canvas(*s.seat()));
    CHECK(std::stoll(s.read("doc_epoch")) > successor_generation);
    CHECK(back.active == "standard");
    CHECK(s.r.kernel.is_loaded(pane::kEditorPaneStem));
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b"));
    CHECK(s.read("text") == "int one;\nint Xtwo;\n");
    CHECK(s.read("caret_byte") == "5");
    CHECK(s.r.plan_->state_of(pane::kEditorPaneStem) == load::RowState::Resolved);
}

TEST_CASE("the editor switch answers its page for every shape it accepts, and its page names no other") {
    namespace manual = zengine::manual;
    SwitchRig s("switch-manual");
    s.open(standard_and_twin());
    const manual::Manual& page = manual::pages::zengine_editor_switch::kManual;
    // A COMMAND IS ANSWERED FROM `Commands`, A COLLABORATOR'S WORD FROM `Hears`, both from the page
    // this build embedded...
    ShapeDocumentReader reader(s.r.bus, kEditorSwitchRole);
    const manual::ShapeDocumentShown asked =
        reader.ask(EditorSwitchRequested::zen_name, EditorSwitchRequested::zen_version);
    CHECK(asked.refusal.empty());
    CHECK(asked.group == "Commands");
    CHECK(asked.page == std::string(page.content_id));
    CHECK(asked.text.find("**Refuses:**") != std::string::npos);
    CHECK(reader.ask(EditorLive::zen_name, EditorLive::zen_version).group == "Hears");
    // ...AND A VERSION THE GATE DOES NOT ACCEPT IS REFUSED WITH NO WORDS.
    const manual::ShapeDocumentShown other = reader.ask(EditorSwitchRequested::zen_name, 2);
    CHECK(other.refusal == "zengine.editor-switch does not accept `EditorSwitchRequested` v2");
    CHECK(other.text.empty());
    // EVERY SHAPE THE OFFICE ACCEPTS IS ON ITS PAGE, AND THE PAGE NAMES NO OTHER.
    CHECK(check_office_manual(s.r.bus, kEditorSwitchRole) > 0);
    check_page_names_accepted(page, described_shapes_of(s.r.bus, s.r.bus.role_holder(kEditorSwitchRole)));
    // ASKING CHANGED NOTHING: no switch began.
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::Idle);
}

TEST_CASE("asking for the choice that already holds the office is harmless: nothing is loaded") {
    SwitchRig s("switch-same");
    s.open(standard_and_twin());
    const loom::WeaveId first = s.holder();
    const EditorSwitchAnswered same = s.switch_to("standard");
    CHECK(same.outcome == switch_outcome::kAlreadyActive);
    CHECK(same.op == 0);
    CHECK(same.active == "standard");
    CHECK(s.holder() == first);
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b"));
    const EditorSwitchAnswered status = s.ask(EditorSwitchStatusRequested{});
    CHECK(status.outcome == switch_outcome::kStatus);
    CHECK(status.active == "standard");
    CHECK(status.choices == std::vector<std::string>{"standard", "twin"});
}

TEST_CASE("a choice the plan does not author, and one whose artifact will not load, are refused with nothing moved") {
    SwitchRig s("switch-refused");
    std::vector<load::ChoiceIntent> choices = standard_and_twin();
    choices.push_back(load::ChoiceIntent{pane::kEditorPaneRole, "absent", "zengine-editor-pane-not-built"});
    s.open(choices);
    const loom::WeaveId first = s.holder();
    const std::string path = s.open_file("kept.cpp", "kept\n");
    const EditorSwitchAnswered unknown = s.switch_to("emacs");
    CHECK(unknown.outcome == switch_outcome::kRefused);
    CHECK(unknown.detail.find("no editor choice named `emacs`") != std::string::npos);
    const EditorSwitchAnswered absent = s.switch_to("absent");
    CHECK(absent.outcome == switch_outcome::kRefused);
    CHECK(absent.detail.find("did not load") != std::string::npos);
    CHECK(s.holder() == first);
    CHECK(s.read("path") == path);
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-not-built"));
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::Idle);
    // THE INCUMBENT WAS NEVER HELD STILL: typing still reaches it.
    s.press_doc(0, 0);
    s.type("y");
    CHECK(s.read("text") == "ykept\n");
}

TEST_CASE("a destination that refuses the document leaves the incumbent editing, resumed and told") {
    SwitchRig s("switch-refusing");
    std::vector<load::ChoiceIntent> choices = standard_and_twin();
    choices.push_back(load::ChoiceIntent{pane::kEditorPaneRole, "refusing", "zengine-editor-pane-refusing"});
    s.open(choices);
    const loom::WeaveId first = s.holder();
    (void)s.open_file("held.cpp", "held\n");
    const EditorSwitchAnswered no = s.switch_to("refusing");
    CHECK(no.outcome == switch_outcome::kRefused);
    CHECK(no.detail.find("could not adopt the document") != std::string::npos);
    CHECK(no.detail.find("refuses every adoption") != std::string::npos);
    CHECK(s.holder() == first);
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-refusing"));
    CHECK(s.shows("the editor switch did not happen"));
    s.press_doc(0, 0);
    s.type("z");
    CHECK(s.read("text") == "zheld\n");
}

TEST_CASE("a switch waiting on a silent candidate is pending, published, refuses a second switch, and cancels cleanly") {
    SUBCASE("the sealed candidate asks no graphics window for a room and draws in none while it "
            "warms; the incumbent keeps drawing") {
        namespace gfx = zengine::graphics;
        namespace pw = zengine::testing::pane_window;
        const std::string area = pane_area(pane::kEditorPaneRole, pane::kEditorPane);
        std::vector<loom::WeaveId> askers; // outlive the rig, whose bus the tap is on
        std::size_t drawers = 0;
        loom::WeaveId first{};
        SwitchRig w("switch-silent-window");
        std::vector<load::ChoiceIntent> choices = standard_and_twin();
        choices.push_back(load::ChoiceIntent{pane::kEditorPaneRole, "silent", "zengine-editor-pane-silent"});
        const loom::ObserverId tap = w.r.bus.add_observer([&](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered && ev.kind != loom::EventKind::Refused) return;
            if (ev.authored_role != pane::kEditorPaneRole) return; // the Editor's office alone
            if (ev.schema_name == gfx::RoomAsked::zen_name) askers.push_back(ev.sender);
            if (ev.schema_name == gfx::Drawn::zen_name && first.valid() && !(ev.sender == first))
                ++drawers;
        });
        w.open(choices);
        first = w.holder();
        REQUIRE(askers.size() == 1);
        CHECK(askers[0] == first);
        pw::Window window(w.r.bus);
        window.room(area, pane::kEditorPaneRole, 40, 10);
        (void)w.open_file("wait.cpp", "wait\n");
        w.enqueue([](SwitchAsker&, loom::Mail& mail) {
            (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchRequested{"silent"});
        });
        w.r.bus.drain_until_idle();
        REQUIRE(w.switcher->stage() == EditorSwitchCoordinator::Stage::Warming);
        REQUIRE(w.r.kernel.is_loaded("zengine-editor-pane-silent"));
        const std::size_t mark = window.weave().heard.size();
        w.press_doc(0, 0);
        w.type("w");
        CHECK(window.weave().heard.size() > mark);
        const std::vector<std::string> rows = window.rows(area);
        CHECK(std::find(rows.begin(), rows.end(), "wwait") != rows.end());
        CHECK(askers.size() == 1);
        CHECK(drawers == 0);
        CHECK(window.weave().strangers == 0);
        const std::int64_t op = w.switcher->op();
        w.enqueue([op](SwitchAsker&, loom::Mail& mail) {
            (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchCancelled{op});
        });
        w.r.bus.drain_until_idle();
        CHECK_FALSE(w.r.kernel.is_loaded("zengine-editor-pane-silent"));
        CHECK(askers.size() == 1);
        w.r.bus.remove_observer(tap);
    }
    SwitchRig s("switch-silent");
    std::vector<load::ChoiceIntent> choices = standard_and_twin();
    choices.push_back(load::ChoiceIntent{pane::kEditorPaneRole, "silent", "zengine-editor-pane-silent"});
    s.open(choices);
    const loom::WeaveId first = s.holder();
    (void)s.open_file("wait.cpp", "wait\n");
    const std::size_t answers = s.asker->answers.size();
    s.enqueue([](SwitchAsker&, loom::Mail& mail) {
        (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchRequested{"silent"});
    });
    s.r.bus.drain_until_idle();
    CHECK(s.asker->answers.size() == answers); // pending: no answer yet
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::Warming);
    const std::int64_t op = s.switcher->op();
    CHECK(s.r.kernel.is_loaded("zengine-editor-pane-silent"));
    CHECK(s.holder() == first);
    // A SECOND REQUEST IS REFUSED IN WORDS, and the first is still pending.
    const EditorSwitchAnswered busy = s.switch_to("twin");
    CHECK(busy.outcome == switch_outcome::kRefused);
    CHECK(busy.detail.find("is under way (warming)") != std::string::npos);
    // THE INCUMBENT IS NOT HELD WHILE THE CANDIDATE WARMS: the weaver keeps typing.
    s.press_doc(0, 0);
    s.type("w");
    CHECK(s.read("text") == "wwait\n");
    // CANCEL: the candidate is discarded, and both asks are answered -- the cancel's own first,
    // then the pending request's.
    s.enqueue([op](SwitchAsker&, loom::Mail& mail) {
        (void)mail.as_role(kSwitchAskerOffice).send_to_role(kEditorSwitchRole, EditorSwitchCancelled{op});
    });
    s.r.bus.drain_until_idle();
    REQUIRE(s.asker->answers.size() == answers + 3); // busy, the cancel's own answer, and the pending request's
    CHECK(s.asker->answers[answers + 1].outcome == switch_outcome::kCancelled);
    CHECK(s.asker->answers[answers + 1].op == op);
    CHECK(s.asker->answers[answers + 2].outcome == switch_outcome::kCancelled);
    CHECK(s.asker->answers[answers + 2].detail.find("was cancelled; nothing moved") != std::string::npos);
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-silent"));
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::Idle);
    CHECK(s.holder() == first);
}

TEST_CASE("a keystroke that reaches the incumbent after its boundary is refused, counted, and reported with the switch") {
    SwitchRig s("switch-boundary");
    s.open(standard_and_twin());
    (void)s.open_file("edge.cpp", "edge\n");
    s.press_doc(0, 0);
    const std::size_t before = s.asker->answers.size();
    s.enqueue_switch("twin");
    // THE BOUNDARY ASK IS QUEUED; the keystroke queued now reaches the incumbent after it has
    // authored the transfer and before anything commits.
    s.pump_until(EditorSwitchCoordinator::Stage::Boundary);
    s.enqueue_typed("Q");
    s.r.bus.drain_until_idle();
    REQUIRE(s.asker->answers.size() == before + 1);
    const EditorSwitchAnswered done = s.asker->answers.back();
    CHECK_MESSAGE(done.outcome == switch_outcome::kSwitched, done.detail);
    CHECK(s.read("text") == "edge\n"); // the transfer is the document at the boundary
    bool reported = false;
    for (const std::string& note : done.notes) {
        reported = reported || note.find("1 input was refused while `standard` held still") != std::string::npos;
    }
    CHECK_MESSAGE(reported, "the refused keystroke was not reported");
    // ...AND IN THE ANSWER'S WORDS, which are what a Terminal's weaver reads.
    CHECK_MESSAGE(done.detail.find("1 input was refused while `standard` held still") != std::string::npos, done.detail);
}

TEST_CASE("a keystroke queued behind the commitment reaches the successor, ahead of nothing it could miss") {
    SwitchRig s("switch-behind");
    s.open(standard_and_twin());
    (void)s.open_file("after.cpp", "after\n");
    s.press_doc(0, 0);
    const std::size_t before = s.asker->answers.size();
    s.enqueue_switch("twin");
    // THE ADOPTION IS ASKED; a keystroke queued now is role traffic the admission is placed
    // ahead of, so the successor -- holding the adopted document -- is the one that types it.
    s.pump_until(EditorSwitchCoordinator::Stage::Adopting);
    s.enqueue_typed("Q");
    s.r.bus.drain_until_idle();
    REQUIRE(s.asker->answers.size() == before + 1);
    CHECK_MESSAGE(s.asker->answers.back().outcome == switch_outcome::kSwitched, s.asker->answers.back().detail);
    CHECK(s.read("text") == "Qafter\n");
    for (const std::string& note : s.asker->answers.back().notes) {
        CHECK(note.find("refused while") == std::string::npos);
    }
}

TEST_CASE("a switch that would lose something loads nothing until the weaver consents, and a consent the losses moved past is asked for again") {
    SwitchRig s("switch-consent");
    const std::vector<load::ChoiceIntent> choices = {
        load::ChoiceIntent{pane::kEditorPaneRole, "losing", "zengine-editor-pane-losing"},
        load::ChoiceIntent{pane::kEditorPaneRole, "twin", "zengine-editor-pane-b"}};
    s.open(choices, "zengine-editor-pane-losing");
    const loom::WeaveId first = s.holder();
    (void)s.open_file("consent.cpp", "one\ntwo\n");

    const EditorSwitchAnswered asked = s.switch_to("twin");
    REQUIRE(asked.outcome == switch_outcome::kNeedsConfirmation);
    CHECK(asked.op > 0);
    CHECK(asked.losses == std::vector<std::string>{"test instrumentation: 3 lines"});
    REQUIRE(asked.consent.size() == 9);
    CHECK(asked.consent[0] == 'c');
    CHECK(asked.active == "losing");
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b")); // nothing loaded before consent
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::AwaitingConsent);
    CHECK(s.holder() == first);
    const EditorSwitchAnswered status = s.ask(EditorSwitchStatusRequested{});
    CHECK(status.consent == asked.consent);
    CHECK(status.losses == asked.losses);

    // A CONSENT THAT IS NOT THE ONE ASKED FOR moves nothing, and the right one is said again.
    const EditorSwitchAnswered wrong = s.ask(EditorSwitchConfirmed{asked.op, "c00000000"});
    CHECK(wrong.outcome == switch_outcome::kNeedsConfirmation);
    CHECK(wrong.consent == asked.consent);
    CHECK(wrong.detail.find("that consent is not the one") != std::string::npos);
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b"));

    // THE WEAVER KEEPS WORKING WHILE THE QUESTION STANDS, and the losses move with the document:
    // the consent given for three lines is recognised at the boundary as stale, nothing crosses,
    // and a fresh consent is asked for.
    s.press_doc(0, 0);
    s.r.key(input::scan::kReturn);
    CHECK(s.read("text") == "\none\ntwo\n");
    const EditorSwitchAnswered stale = s.ask(EditorSwitchConfirmed{asked.op, asked.consent});
    REQUIRE(stale.outcome == switch_outcome::kNeedsConfirmation);
    CHECK(stale.op == asked.op);
    CHECK(stale.losses == std::vector<std::string>{"test instrumentation: 4 lines"});
    CHECK_FALSE(stale.consent == asked.consent);
    CHECK(stale.detail.find("changed before it could begin") != std::string::npos);
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b")); // the candidate was discarded
    CHECK(s.holder() == first);
    CHECK(s.shows("the editor switch did not happen"));
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::AwaitingConsent);

    const EditorSwitchAnswered done = s.ask(EditorSwitchConfirmed{asked.op, stale.consent});
    CHECK_MESSAGE(done.outcome == switch_outcome::kSwitched, done.detail);
    CHECK(done.active == "twin");
    CHECK_FALSE(s.holder() == first);
    CHECK(s.read("text") == "\none\ntwo\n");
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-losing"));
}

TEST_CASE("a newer request replaces a switch awaiting consent, and the replaced switch's confirmation is answered superseded") {
    SwitchRig s("switch-superseded");
    const std::vector<load::ChoiceIntent> choices = {
        load::ChoiceIntent{pane::kEditorPaneRole, "losing", "zengine-editor-pane-losing"},
        load::ChoiceIntent{pane::kEditorPaneRole, "twin", "zengine-editor-pane-b"}};
    s.open(choices, "zengine-editor-pane-losing");
    (void)s.open_file("replaced.cpp", "replaced\n");
    const EditorSwitchAnswered older = s.switch_to("twin");
    REQUIRE(older.outcome == switch_outcome::kNeedsConfirmation);
    const EditorSwitchAnswered newer = s.switch_to("twin");
    REQUIRE(newer.outcome == switch_outcome::kNeedsConfirmation);
    CHECK(newer.op > older.op);

    const EditorSwitchAnswered late = s.ask(EditorSwitchConfirmed{older.op, older.consent});
    CHECK(late.outcome == switch_outcome::kSuperseded);
    CHECK(late.op == older.op);
    CHECK(late.detail == "switch " + std::to_string(older.op) + " was replaced by switch " +
                             std::to_string(newer.op) + " before it was confirmed");
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b"));
    CHECK(s.switcher->op() == newer.op); // the newer switch still stands

    const EditorSwitchAnswered cancelled = s.ask(EditorSwitchCancelled{newer.op});
    CHECK(cancelled.outcome == switch_outcome::kCancelled);
    CHECK(s.switcher->stage() == EditorSwitchCoordinator::Stage::Idle);
    CHECK(s.read("text") == "replaced\n");
}

TEST_CASE("the documented Terminal lines, typed through Workshop's own door, ask, confirm and switch, and the desk shows the switch while it stands") {
    SwitchRig s("switch-terminal");
    const std::vector<load::ChoiceIntent> choices = {
        load::ChoiceIntent{pane::kEditorPaneRole, "losing", "zengine-editor-pane-losing"},
        load::ChoiceIntent{pane::kEditorPaneRole, "twin", "zengine-editor-pane-b"}};
    s.open(choices, "zengine-editor-pane-losing");
    s.mount_terminal();
    const loom::WeaveId first = s.holder();
    (void)s.open_file("typed.cpp", "typed\n");

    // THE STATUS LINE, exactly as documented.
    const EditorSwitchAnswered status = s.typed("ask @zengine.editor-switch EditorSwitchStatusRequested 1");
    CHECK(status.outcome == switch_outcome::kStatus);
    CHECK(status.active == "losing");
    CHECK(status.choices == std::vector<std::string>{"losing", "twin"});
    CHECK_MESSAGE(s.r.session().notice ==
                      "editor switch: status -- `losing` holds zengine.editor (choices: losing, twin); no switch is under way",
                  s.r.session().notice);

    // THE REQUEST LINE: this incumbent loses something, so the answer asks for consent, and the
    // desk keeps the switch as a condition that says how to stop it.
    const EditorSwitchAnswered asked = s.typed("ask @zengine.editor-switch EditorSwitchRequested 1 destination=twin");
    REQUIRE(asked.outcome == switch_outcome::kNeedsConfirmation);
    CHECK(s.holder() == first);
    const std::string key = "editor-switch:" + std::to_string(asked.op);
    {
        const std::vector<Condition> standing = attention_conditions(s.r.session());
        const Condition* shown = condition_by_key(standing, key);
        REQUIRE_MESSAGE(shown != nullptr, "the pending switch is not a standing condition");
        CHECK(shown->compact == "switching the Editor to twin");
        CHECK(shown->detail.find("ask @zengine.editor-switch EditorSwitchCancelled 1 op=" +
                                 std::to_string(asked.op)) != std::string::npos);
        // ...AND THE EXACT CONFIRMATION LINE, consent included: the Terminal shows an answer's shape and
        // not its fields, so the desk is where a weaver reads the consent to type.
        CHECK(shown->detail.find("ask @zengine.editor-switch EditorSwitchConfirmed 1 op=" +
                                 std::to_string(asked.op) + " consent=" + asked.consent) != std::string::npos);
    }
    // ...AND THE NOTICE LINE ASKS AT ONCE, the line to type first -- a notice is cut to the band's
    // width -- then what the switch would lose, in the answer's words.
    CHECK_MESSAGE(s.r.session().notice.rfind("editor switch " + std::to_string(asked.op) +
                                                 ": needs-confirmation -- confirm: ask @zengine.editor-switch "
                                                 "EditorSwitchConfirmed 1 op=" + std::to_string(asked.op) +
                                                 " consent=" + asked.consent + " -- ",
                                             0) == 0,
                  s.r.session().notice);
    CHECK(s.r.session().notice.find("would lose test instrumentation: 2 lines") != std::string::npos);
    CHECK_FALSE(s.r.session().notice_is_bad);

    // THE CONFIRMATION LINE, with the op and consent the answer carried -- a consent begins with a
    // letter, so the Terminal's grammar reads it as text.
    const EditorSwitchAnswered done = s.typed("ask @zengine.editor-switch EditorSwitchConfirmed 1 op=" +
                                              std::to_string(asked.op) + " consent=" + asked.consent);
    CHECK_MESSAGE(done.outcome == switch_outcome::kSwitched, done.detail);
    CHECK(done.active == "twin");
    CHECK_FALSE(s.holder() == first);
    CHECK(s.read("text") == "typed\n");
    CHECK(condition_by_key(attention_conditions(s.r.session()), key) == nullptr);
    // WHAT CAME OF IT IS SAID ON THE NOTICE LINE, in the answer's own words.
    CHECK_MESSAGE(s.r.session().notice.rfind("editor switch " + std::to_string(asked.op) + ": switched -- ", 0) == 0,
                  s.r.session().notice);

    // AND ASKING FOR WHAT IS ALREADY ACTIVE IS HARMLESS, from the Terminal too -- and said so.
    const EditorSwitchAnswered again = s.typed("ask @zengine.editor-switch EditorSwitchRequested 1 destination=twin");
    CHECK(again.outcome == switch_outcome::kAlreadyActive);
    CHECK_MESSAGE(s.r.session().notice.rfind("editor switch: already-active -- ", 0) == 0, s.r.session().notice);
}

TEST_CASE("a successor that holds the office and does not serve is a failure after the commitment, and the retired Editor is kept") {
    SUBCASE("with a graphics window showing the office's area, the failure after the commitment and "
            "the switch home end as they do without one, and the plan's own Editor is drawn there "
            "again whole") {
        namespace pw = zengine::testing::pane_window;
        const std::string area = pane_area(pane::kEditorPaneRole, pane::kEditorPane);
        SwitchRig w("switch-not-live-window");
        std::vector<load::ChoiceIntent> choices = standard_and_twin();
        choices.push_back(load::ChoiceIntent{pane::kEditorPaneRole, "not-live", "zengine-editor-pane-not-live"});
        w.open(choices);
        pw::Window window(w.r.bus);
        window.room(area, pane::kEditorPaneRole, 40, 10);
        (void)w.open_file("after.cpp", "after\n");
        const EditorSwitchAnswered failed = w.switch_to("not-live");
        CHECK(failed.outcome == switch_outcome::kFailedAfterCommit);
        CHECK(w.r.kernel.is_loaded(pane::kEditorPaneStem)); // retired, sealed, kept
        const EditorSwitchAnswered home = w.switch_to("standard");
        CHECK_MESSAGE(home.outcome == switch_outcome::kSwitched, home.detail);
        const std::size_t mark = window.weave().heard.size();
        window.room(area, pane::kEditorPaneRole, 40, 10);
        REQUIRE(window.weave().heard.size() == mark + 1);
        CHECK(window.weave().heard.back().whole);
        const std::vector<std::string> rows = window.rows(area);
        CHECK(std::find(rows.begin(), rows.end(), "after") != rows.end());
        CHECK(window.weave().strangers == 0);
    }
    SwitchRig s("switch-not-live");
    std::vector<load::ChoiceIntent> choices = standard_and_twin();
    choices.push_back(load::ChoiceIntent{pane::kEditorPaneRole, "not-live", "zengine-editor-pane-not-live"});
    s.open(choices);
    (void)s.open_file("after.cpp", "after\n");
    const EditorSwitchAnswered failed = s.switch_to("not-live");
    CHECK(failed.outcome == switch_outcome::kFailedAfterCommit);
    CHECK(failed.detail.find("never serves") != std::string::npos);
    CHECK(failed.detail.find("was kept") != std::string::npos);
    CHECK(s.r.kernel.is_loaded("zengine-editor-pane-not-live"));
    CHECK(s.r.kernel.is_loaded(pane::kEditorPaneStem)); // retired, sealed, kept
    CHECK(s.r.plan_->choice_holder(pane::kEditorPaneRole) == "zengine-editor-pane-not-live");
    // THE DOCUMENT IS STILL WHOLE IN THE SUCCESSOR, and switching back carries it home and
    // releases the kept Editor before loading the plan's own image again.
    CHECK(s.read("text") == "after\n");
    const EditorSwitchAnswered home = s.switch_to("standard");
    CHECK_MESSAGE(home.outcome == switch_outcome::kSwitched, home.detail);
    CHECK(s.read("text") == "after\n");
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-not-live"));
}

} // TEST_SUITE
