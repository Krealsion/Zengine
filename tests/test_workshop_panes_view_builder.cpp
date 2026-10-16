// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the View Builder, as a loaded weave: the rows it declares, held to
// its census (tests/actions/zengine.view.builder.txt), and the manual it answers, held to those
// rows. Its edits, canvas and files are proved in `view_builder` (tests/test_view_builder.cpp).

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "census.hpp"
#include "view/view-builder/vocabulary.hpp"

#include <set>
#include <string>

namespace {

namespace vb = zengine::view_builder;

/// A LIVE WORKSHOP WITH THE REAL VIEW BUILDER LOADED INTO IT, as its plan row loads it: the rows
/// Workshop holds of its one pane.
const RuntimePane* load_view_builder(PaneRig& r) {
    r.mount_workshop();
    r.ready();
    r.extent(320, 100); // room for the builder's canvas
    REQUIRE(r.load("zengine-view-builder", WORKSHOP_SO_VIEW_BUILDER, vb::kRole).valid());
    REQUIRE(r.load_refusals.empty());
    const RuntimePane* held = r.session().panes.runtime.find(vb::kRole, vb::kPane);
    REQUIRE_MESSAGE(held != nullptr, "the loaded image offered no `view-builder` pane");
    REQUIRE_FALSE(held->actions.empty());
    return held;
}

/// THE VIEW BUILDER'S CENSUS: its rows, in the one mode it has, which declares none.
const CensusDriverOf kViewBuilderCensus{vb::kRole, [] {
    OfficeCensus census;
    PaneRig r;
    const RuntimePane* held = load_view_builder(r);
    CHECK(held->mode.empty());
    CHECK(held->elsewhere.empty());
    census_take(census, vb::kRole, *held);
    return census;
}};

/// THE VIEW BUILDER'S PRESSES: each of its rows, on a new view.
const PressDriverOf kViewBuilderPresses{vb::kRole, [](PressWitness& w) {
    press_each_state<PaneRig>(
        w, [] { return std::make_unique<PaneRig>(); },
        [](PaneRig& r, const std::function<void(const RuntimePane&)>& at) {
            at(*load_view_builder(r));
        });
}};

} // namespace

TEST_CASE("the View Builder answers the manual its build embedded, a section for every action it "
          "declares and none for another") {
    PaneRig r;
    const RuntimePane* held = load_view_builder(r);
    std::set<std::string> declared;
    for (const v3::PaneActionRow& a : held->actions) declared.insert(a.id);
    ManualReader reader(r, vb::kRole);
    check_answered_manual(reader, vb::kPane, declared);
    const PaneDocumentShown other = reader.ask("elsewhere", "");
    CHECK(other.refusal == "zengine.view.builder offers no pane `elsewhere`");
    CHECK(other.sections.empty());
}
