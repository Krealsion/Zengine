// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the View Builder, as a loaded weave: the rows Workshop holds of it,
// held to its census (tests/actions/zengine.view.builder.txt). The manual it answers, held to
// those rows, is proved beside a stand-in for Workshop in `panes_apart` (tests/test_panes_apart.cpp);
// its edits, canvas and files in `view_builder` (tests/test_view_builder.cpp).

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "census.hpp"
#include "view/view-builder/vocabulary.hpp"

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
    RoleCensus census;
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

ZEN_PRESS_WITNESS_CASE(vb::kRole, 0, "the View Builder's rows")

} // namespace
