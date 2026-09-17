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

namespace {

inline std::vector<load::ChoiceIntent> standard_and_twin() {
    return {load::ChoiceIntent{pane::kEditorPaneRole, "standard", pane::kEditorPaneStem},
            load::ChoiceIntent{pane::kEditorPaneRole, "twin", "zengine-editor-pane-b"}};
}

} // namespace

TEST_SUITE("workshop_editor_switch") {

TEST_CASE("a switch between two authored Editors carries the document, its unsaved edits and its caret, and moves the office") {
    SwitchRig s("switch-carry");
    s.open(standard_and_twin());
    const loom::WeaveId first = s.holder();
    const std::string path = s.open_file("carry.cpp", "int one;\nint two;\n");
    s.press_doc(1, 4);
    s.type("X");
    REQUIRE(s.status().rfind("UNSAVED", 0) == 0);

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
    // THE DESK SHOWS THE SUCCESSOR IN THE SAME SEAT.
    CHECK(s.r.session().panels.has(s.kind));
    CHECK(s.shows("UNSAVED"));
    CHECK(s.shows("int Xtwo;"));

    // AND BACK, through the same law, to the plan's own row.
    const EditorSwitchAnswered back = s.switch_to("standard");
    CHECK_MESSAGE(back.outcome == switch_outcome::kSwitched, back.detail);
    CHECK(back.active == "standard");
    CHECK(s.r.kernel.is_loaded(pane::kEditorPaneStem));
    CHECK_FALSE(s.r.kernel.is_loaded("zengine-editor-pane-b"));
    CHECK(s.read("text") == "int one;\nint Xtwo;\n");
    CHECK(s.read("caret_byte") == "5");
    CHECK(s.r.plan_->state_of(pane::kEditorPaneStem) == load::RowState::Resolved);
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
    // THE INCUMBENT IS NOT HELD WHILE THE CANDIDATE WARMS: the maker keeps typing.
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

TEST_CASE("a switch that would lose something loads nothing until the maker consents, and a consent the losses moved past is asked for again") {
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

    // THE MAKER KEEPS WORKING WHILE THE QUESTION STANDS, and the losses move with the document: the
    // consent given for three lines is recognised at the boundary as stale, nothing crosses, and a
    // fresh consent is asked for.
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
    }

    // THE CONFIRMATION LINE, with the op and consent the answer carried -- a consent begins with a
    // letter, so the Terminal's grammar reads it as text.
    const EditorSwitchAnswered done = s.typed("ask @zengine.editor-switch EditorSwitchConfirmed 1 op=" +
                                              std::to_string(asked.op) + " consent=" + asked.consent);
    CHECK_MESSAGE(done.outcome == switch_outcome::kSwitched, done.detail);
    CHECK(done.active == "twin");
    CHECK_FALSE(s.holder() == first);
    CHECK(s.read("text") == "typed\n");
    CHECK(condition_by_key(attention_conditions(s.r.session()), key) == nullptr);

    // AND ASKING FOR WHAT IS ALREADY ACTIVE IS HARMLESS, from the Terminal too.
    const EditorSwitchAnswered again = s.typed("ask @zengine.editor-switch EditorSwitchRequested 1 destination=twin");
    CHECK(again.outcome == switch_outcome::kAlreadyActive);
}

TEST_CASE("a successor that holds the office and does not serve is a failure after the commitment, and the retired Editor is kept") {
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
