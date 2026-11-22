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
#include "input/vocabulary.hpp"
#include "pane/drawing.hpp"
#include "terminal/vocabulary.hpp"
#include "view/view-builder/vocabulary.hpp"

#include <algorithm>
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

// ============================================================================
// Tally, the example pane (examples/tally-pane/tally.cpp): each case is a body over the image it
// loads, run on the native build, and under the `box` gate on the module built from the same
// source, through the same kernel, so the boxed pane answers as the native one does or a twin
// goes red.
// ============================================================================

namespace {

namespace scan = zengine::input::scan;

constexpr const char* kTallyRole = "example.tally";
constexpr const char* kTallyPane = "tally";
constexpr const char* kTallyNativeStem = "zengine-example-tally";
/// The same pane as a module: a stem spelled with WebAssembly's suffix.
constexpr const char* kTallyBoxedStem = "zengine-example-tally.wasm";

/// TALLY LOADED AS A PLAN ROW LOADS IT, from `image` as `stem`, and its area shown: a window of
/// characters gives it room to draw in, as the desk gives the pane a weaver picked.
struct TallyStandIn {
    PaneStandInRig r;
    std::string stem;
    std::string area = zengine::pane::pane_area(kTallyRole, kTallyPane);

    TallyStandIn(const char* name, const char* image) : stem(name) {
        INFO("refusals: ", r.refusals.empty() ? std::string() : r.refusals.back());
        REQUIRE(r.load(stem, image, kTallyRole).valid());
        const std::string offered = std::string(zengine::pane::PaneOffered::zen_name) + "/" +
                                    std::to_string(zengine::pane::PaneOffered::zen_version);
        REQUIRE(std::find(r.to_workshop.begin(), r.to_workshop.end(), offered) != r.to_workshop.end());
        r.show(kTallyRole, area, 60, 12);
    }
    std::string text() const { return r.picture(area); }
    /// A key Workshop sends the pane while it holds the keyboard.
    void key(std::int64_t scancode) { r.key(kTallyRole, kTallyPane, scancode); }
};

void offers_its_pane_and_counts(const char* name, const char* image) {
    TallyStandIn t(name, image);
    // ITS PANE, BY THE NAME IT OFFERED, DRAWING ITS COUNT FROM ZERO, and the one action it declared.
    CHECK(t.r.actions == std::vector<std::string>{"tally.add"});
    CHECK_MESSAGE(t.text().find("Tally: 0") != std::string::npos, t.text());
    CHECK(t.text().find("press into me") != std::string::npos);
    // EACH SPACE ADDS ONE, AND THE PICTURE SAYS SO.
    t.key(scan::kSpace);
    CHECK_MESSAGE(t.text().find("Tally: 1") != std::string::npos, t.text());
    t.key(scan::kSpace);
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
    // ...AND ANOTHER KEY ADDS NOTHING: only the action it declared reaches it, and the key itself
    // is refused at its door, which takes no key.
    t.key(scan::kA);
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
    REQUIRE_FALSE(t.r.from_workshop.empty());
    CHECK(t.r.from_workshop.back() == std::string(zengine::pane::PaneKey::zen_name) + "/1 refused");
}

void keeps_its_count_across_a_reload_in_place(const char* name, const char* image) {
    TallyStandIn t(name, image);
    t.key(scan::kSpace);
    t.key(scan::kSpace);
    REQUIRE(t.text().find("Tally: 2") != std::string::npos);
    // THE SAME IMAGE, REBUILT: its state rides the reload, and the reloaded pane draws it.
    const loom::WeaveId before = t.r.weave_id(t.stem);
    const loom::ReloadResult reloaded = t.r.reload(t.stem, t.r.copy_of(image, "rebuilt-"));
    REQUIRE_MESSAGE(reloaded.ok, reloaded.error);
    CHECK(t.r.weave_id(t.stem) == before);
    CHECK_MESSAGE(t.text().find("Tally: 2") != std::string::npos, t.text());
    // ...AND IT GOES ON COUNTING FROM THERE.
    t.key(scan::kSpace);
    CHECK_MESSAGE(t.text().find("Tally: 3") != std::string::npos, t.text());
}

} // namespace

TEST_CASE("Tally loaded by a plan row offers its pane, and each Space a weaver gives it adds one") {
    offers_its_pane_and_counts(kTallyNativeStem, PANE_SO_TALLY);
}

TEST_CASE("Tally keeps its count across a reload in place and goes on counting") {
    keeps_its_count_across_a_reload_in_place(kTallyNativeStem, PANE_SO_TALLY);
}

#ifdef PANE_WASM_TALLY
TEST_CASE("boxed: Tally loaded by a plan row offers its pane, and each Space a weaver gives it "
          "adds one") {
    offers_its_pane_and_counts(kTallyBoxedStem, PANE_WASM_TALLY);
}

TEST_CASE("boxed: Tally keeps its count across a reload in place and goes on counting") {
    keeps_its_count_across_a_reload_in_place(kTallyBoxedStem, PANE_WASM_TALLY);
}
#endif
