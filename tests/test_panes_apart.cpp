// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The panes apart from Workshop: cases whose subject is a pane, each loading the pane's built
// image beside a stand-in that holds Workshop's role (tests/pane_stand_in.hpp), and reading only
// what the pane says on the bus and what it answers. No Workshop is compiled in or loaded; a case
// whose subject is Workshop's side of the seam is in Workshop's own suites.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "pane_stand_in.hpp"

#include "attention/vocabulary.hpp"
#include "terminal/vocabulary.hpp"
#include "view/view-builder/vocabulary.hpp"

#include <set>
#include <string>

using zengine::testing::PaneStandInRig;
using zengine::testing::StandInManualReader;

namespace {

/// A PANE LOADED BESIDE THE STAND-IN, as a plan row loads it: the rows it said to Workshop's role.
std::set<std::string> load_and_declared(PaneStandInRig& r, const char* stem, const char* path,
                                        const char* role) {
    INFO("refusals: ", r.refusals.empty() ? std::string() : r.refusals.back());
    REQUIRE(r.load(stem, path, role).valid());
    std::string heard;
    for (const std::string& s : r.to_workshop) heard += s + "; ";
    INFO("said to Workshop's role: ", heard);
    REQUIRE_FALSE(r.actions.empty());
    return std::set<std::string>(r.actions.begin(), r.actions.end());
}

} // namespace

TEST_CASE("the View Builder answers the manual its build embedded, a section for every action it "
          "declares and none for another") {
    namespace vb = zengine::view_builder;
    PaneStandInRig r;
    const std::set<std::string> declared =
        load_and_declared(r, "zengine-view-builder", PANE_SO_VIEW_BUILDER, vb::kRole);
    StandInManualReader reader{r, vb::kRole};
    check_answered_manual(reader, vb::kPane, declared);
    const zengine::pane::PaneDocumentShown other = reader.ask("elsewhere", "");
    CHECK(other.refusal == "zengine.view.builder offers no pane `elsewhere`");
    CHECK(other.sections.empty());
}

TEST_CASE("the manual the Attention pane answers names exactly the commands it declares") {
    namespace pane = zengine::attention_pane;
    PaneStandInRig r;
    const std::set<std::string> declared =
        load_and_declared(r, "zengine-attention-pane", PANE_SO_ATTENTION, pane::kAttentionPaneRole);
    StandInManualReader reader{r, pane::kAttentionPaneRole};
    check_answered_manual(reader, pane::kAttentionPane, declared);
}

TEST_CASE("the manual the Terminal answers names exactly the commands it declares") {
    namespace pane = zengine::terminal_pane;
    PaneStandInRig r;
    const std::set<std::string> declared =
        load_and_declared(r, "zengine-terminal-pane", PANE_SO_TERMINAL, pane::kTerminalPaneRole);
    StandInManualReader reader{r, pane::kTerminalPaneRole};
    check_answered_manual(reader, pane::kTerminalPane, declared);
}
