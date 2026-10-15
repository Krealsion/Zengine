// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- the Builder, as a loaded weave: showing what the tool says,
// choosing and building a recipe, load-after-build, promoting and reverting an image, building
// the frontier, authoring a plan row and opening a recipe's source, driven through the real
// `zengine-builder-pane` image over the real pane protocol against the real doors. A case asserts
// what a weaver can see -- the rows, the message on the bus -- and every gesture is spent while the
// pane holds the keyboard, since its rows are active only then.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"
#include "guest_hand.hpp"
#include "builder_rig.hpp"
#include "census.hpp"

#include "builder/builder-pane/vocabulary.hpp"
#include "workshop/authoring.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"
#include "workshop/pane_document.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/pane_migration.hpp"

#include "manuals/zengine.builder-pane.hpp" // the pane's page, as this tree's build embedded it

#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <set>

namespace {

namespace pane = zengine::builder_pane;
namespace bld = zengine::builder;

using builder_rig::BuilderRig;
using builder_rig::Tool;
using builder_rig::ToolState;
using builder_rig::builder_ref;
using builder_rig::catalog_of;

/// The nine ids the pane declares while it is browsing, and the ones its role line does --
/// spelled through the package's own header so a case cannot agree with a typo.
const std::vector<std::string> kBrowsingIds = {
    pane::kActionBuild,      pane::kActionBuildRealize, pane::kActionPromote,
    pane::kActionRevert,     pane::kActionLoadIt,       pane::kActionRecipeNext,
    pane::kActionRecipeBack, pane::kActionFrontier,     pane::kActionEditSource};

/// ...and how many the pane declares while browsing: those nine, the output reader's own, and
/// five more -- the recipe list, this pane's own menu, and the two halves of load-after-build,
/// which carry no default key so a control and a menu row can each name one operation.
const std::vector<std::string> kAddedBrowsingIds = {pane::kActionOutput, pane::kActionRecipes,
                                                    pane::kActionMenu, pane::kActionArm,
                                                    pane::kActionLoadBuilt};
const std::size_t kBrowsingRows = kBrowsingIds.size() + kAddedBrowsingIds.size();

/// The rows the role line declares: its commit, this pane's menu and its cancel.
const std::vector<std::string> kRoleLineIds = {pane::kActionCommit, pane::kActionMenu,
                                               pane::kActionCancel};

} // namespace

// ============================================================================
// BLD-WEAVE — the pane arrives, and it is a stranger
// ============================================================================

TEST_CASE("the Builder arrives by a plan row, under an office of its own") {
    // Workshop compiled nothing for this pane, minted no kind for it and holds no branch on it:
    // what puts it on a weaver's screen is a row in an editable file naming an artifact, and an
    // offer this host learns about at runtime like any other.
    BuilderRig b("bld-arrive");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    const RuntimePane* seat = b.row();
    REQUIRE(seat != nullptr);
    CHECK(seat->provider == std::string(pane::kBuilderPaneRole));
    CHECK(seat->pane == std::string(pane::kBuilderPane));
    CHECK(seat->name == std::string(pane::kBuilderPaneName));
    CHECK(seat->summary == std::string(pane::kBuilderPaneSummary));
    // A RUNTIME HANDLE, minted from a live offer -- not a compile-time kind.
    CHECK(is_runtime_kind(seat->kind));
    // ...AND THE HOST'S OWN CATALOG OFFERS NO SUCH PANE.
    for (const BuiltinPane& row : kBuiltinPanes) {
        CHECK(std::string(row.pane) != std::string(pane::kBuilderPane));
        CHECK(std::string(row.name) != std::string(pane::kBuilderPaneName));
    }
}

TEST_CASE("the pane asks the tool what it is on its own room grant, and shows it") {
    BuilderRig b("bld-open");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.tool->next.outcome = bld::outcome::kNeverBuilt;
    b.open();

    // THE ASK IS THE PANE'S, IN ITS OWN IMAGE. The host sends nothing on its behalf: the
    // pane asks when it is granted a room, which is the only beat it draws on.
    CHECK(b.tool->described >= 1);
    const std::string shown = b.text();
    CHECK(shown.find(std::string("BUILDER @") + bld::kBuilderRole) != std::string::npos);
    CHECK(shown.find("snake -> zengine-snake  (1/1)") != std::string::npos);
    CHECK(shown.find("not built yet") != std::string::npos);
}

TEST_CASE("the pane declares the nine ids a weaver's keymap file already names") {
    // A WEAVER'S OVERRIDE KEEPS WORKING: `builder.build` and its eight neighbours are the pane's
    // own ids, spelled as a keymap file names them, so an authored override that moved
    // `builder.build` keeps moving it.
    BuilderRig b("bld-rows");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    const RuntimePane* seat = b.row();
    REQUIRE(seat != nullptr);
    std::vector<std::string> declared;
    for (const v3::PaneActionRow& row : seat->actions) {
        declared.push_back(row.id);
    }
    for (const std::string& want : kBrowsingIds) {
        INFO("id ", want);
        CHECK(std::find(declared.begin(), declared.end(), want) != declared.end());
    }
    // ...AND THE HOST DECLARES NONE OF THEM. A row in both catalogs would be one authored
    // override naming two things, which `join_pane_rows` refuses whole (WL-KEY-06/08).
    for (const std::string& gone : kBrowsingIds) {
        INFO("id ", gone);
        CHECK(row_of_id(gone.c_str()) == nullptr);
    }
    CHECK(row_of_id("authoring.commit") == nullptr);
    CHECK(row_of_id("authoring.cancel") == nullptr);
}

TEST_CASE("`b` builds only after the weaver has pressed into the pane") {
    // THE NINE ROWS ARE THE PANE'S, so the same key from anywhere else reaches nobody -- `b`
    // builds only with the Builder holding the keys, and nothing stands in its place elsewhere.
    BuilderRig b("bld-focus");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    b.unfocus();
    b.letter(input::scan::kB, "b");
    b.letter(input::scan::kF, "f");
    b.letter(input::scan::kO, "o");
    b.letter(input::scan::kC, "c");
    CHECK(b.tool->asked.empty());
    CHECK(b.authored.empty());

    b.focus();
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    CHECK(b.tool->realize_asked[0] == false);
}

// ============================================================================
// BLD-WEAVE — the catalog, the choice, and the build
// ============================================================================

TEST_CASE("the choice moves with `c`, wraps, and asks for nothing") {
    BuilderRig b("bld-choose");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "c"}});
    b.open();

    CHECK(b.text().find("one -> a  (1/3)") != std::string::npos);
    b.letter(input::scan::kC, "c");
    CHECK(b.text().find("two -> b  (2/3)") != std::string::npos);
    b.letter(input::scan::kC, "c");
    CHECK(b.text().find("three -> c  (3/3)") != std::string::npos);
    b.letter(input::scan::kC, "c");
    CHECK(b.text().find("one -> a  (1/3)") != std::string::npos); // wrapped
    // ...and backwards, on the chorded sibling.
    b.r.key(input::scan::kC, input::mod::kShift);
    CHECK(b.text().find("three -> c  (3/3)") != std::string::npos);
    // NOTHING WAS ASKED OF THE TOOL: choosing is a weaver's act on a presentation.
    CHECK(b.tool->asked.empty());
}

TEST_CASE("`b` builds the recipe the weaver chose, by name") {
    BuilderRig b("bld-chosen");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();

    b.letter(input::scan::kC, "c");
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "two");
}

TEST_CASE("the choice follows its RECIPE to a new row, not its index") {
    // THE CHOICE IS A NAME (WL-PROJ-07): the pane holds the chosen recipe's NAME, so a catalog
    // that comes back reordered moves the choice with no work at all -- there is no index to
    // carry wrongly, which is why the state field is a string.
    BuilderRig b("bld-reorder");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "c"}});
    b.open();

    b.letter(input::scan::kC, "c"); // on `two`
    REQUIRE(b.text().find("two -> b  (2/3)") != std::string::npos);

    b.tool->catalog = catalog_of({{"three", "c"}, {"two", "b"}, {"one", "a"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    CHECK(b.text().find("two -> b  (2/3)") != std::string::npos); // same recipe, same row here
    b.tool->catalog = catalog_of({{"three", "c"}, {"one", "a"}, {"two", "b"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    CHECK(b.text().find("two -> b  (3/3)") != std::string::npos); // it MOVED with the recipe
}

TEST_CASE("a choice whose recipe is gone is released, not inherited") {
    BuilderRig b("bld-gone");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    b.letter(input::scan::kC, "c");
    REQUIRE(b.text().find("two -> b  (2/2)") != std::string::npos);

    b.tool->catalog = catalog_of({{"one", "a"}, {"three", "c"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    // Back to the catalog's own first row -- which is where the pane put them, not where
    // they went, so the frontier action must not read it as an explicit pick (below).
    CHECK(b.text().find("one -> a  (1/2)") != std::string::npos);

    // ...AND THE PICK WENT WITH ITS RECIPE. `two` comes back as the one producer of what the
    // project waits on, `f` chooses it, and a second producer arrives: the pick the weaver made
    // before `two` left is no pick between the two.
    b.frontier.waiting = true;
    b.frontier.artifact = "b";
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    b.letter(input::scan::kF, "f");
    REQUIRE(b.tool->asked.size() == 1);
    REQUIRE(b.tool->asked[0] == "two");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"four", "b"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    b.letter(input::scan::kF, "f");
    CHECK(b.tool->asked.size() == 1);
    CHECK(b.text().find("2 recipes produce `b`") != std::string::npos);
}

TEST_CASE("an empty catalog is said plainly, and `b` asks for nothing") {
    BuilderRig b("bld-empty");
    b.tool->catalog = catalog_of({});
    b.open();

    CHECK(b.text().find("this project has no build recipes") != std::string::npos);
    b.letter(input::scan::kB, "b");
    CHECK(b.tool->asked.empty());
    CHECK(b.text().find("nothing was asked for") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — the pane says which catalog is in force
// ============================================================================

TEST_CASE("the pane names the catalog in force, from RecipeCatalog v2") {
    // `RecipeCatalog` v2 carries `source` from the one owner that holds it beside the rows, and
    // this pane is its consumer: the row names the authored catalog in force.
    BuilderRig b("bld-catalog");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}}, "/project/build/recipes.json");
    b.open();

    CHECK(b.text().find("/project/build/recipes.json") != std::string::npos);

    // ...AND IT MOVES WHEN THE CATALOG DOES, in the same answer as the rows -- which is why
    // it is one shape: a pane can never show one catalog's rows under another's name.
    b.tool->catalog = catalog_of({{"other", "zengine-other"}}, "/project/other.json");
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    CHECK(b.text().find("/project/other.json") != std::string::npos);
    CHECK(b.text().find("/project/build/recipes.json") == std::string::npos);
    CHECK(b.text().find("other -> zengine-other") != std::string::npos);

    // AN ABSENT CATALOG IS SAID BY SAYING NOTHING, which is the owner's own designed
    // absence carried verbatim: a project with nothing to build is a project.
    b.tool->catalog = catalog_of({}, std::string());
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    CHECK(b.text().find("/project/") == std::string::npos);
}

// ============================================================================
// BLD-WEAVE — one action in two states, and the two acts a reload leaves
// ============================================================================

TEST_CASE("`B` before a build is the toggle, and `b` reads it") {
    BuilderRig b("bld-arm");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    b.r.key(input::scan::kB, input::mod::kShift);
    CHECK(b.text().find("load after build: on") != std::string::npos);
    CHECK(b.tool->asked.empty()); // the toggle sends nothing

    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->realize_asked.size() == 1);
    CHECK(b.tool->realize_asked[0] == true);

    // ...AND IT TOGGLES BACK OFF, after which `b` is a plain build again.
    b.r.key(input::scan::kB, input::mod::kShift);
    CHECK(b.text().find("load after build: off") != std::string::npos);
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->realize_asked.size() == 2);
    CHECK(b.tool->realize_asked[1] == false);
}

TEST_CASE("after a plain build that worked, `B` is the button") {
    BuilderRig b("bld-button");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    b.letter(input::scan::kB, "b");
    // THE BUILD ENDS, on the tool's own beat: the ask put it in a condition it had not left,
    // and this is the ending arriving as a separate publication -- which is what makes the
    // button's three preconditions true.
    b.tool->next.artifact = "zengine-snake";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kNotAsked;
    b.tool_says();
    REQUIRE(b.tool->asked.size() == 1);
    CHECK_MESSAGE(b.text().find("loads zengine-snake now") != std::string::npos, b.text());

    // THE BUTTON RE-SENDS THE FINISHED BUILD'S OWN ASK, with the second intention aboard --
    // `shown.recipe`, never the cursor's row, because the thing that is ready is the thing
    // that was built.
    b.r.key(input::scan::kB, input::mod::kShift);
    REQUIRE(b.tool->asked.size() == 2);
    CHECK(b.tool->asked[1] == "snake");
    CHECK(b.tool->realize_asked[1] == true);
}

TEST_CASE("`P` and `R` are one offer each, about the built artifact") {
    BuilderRig b("bld-promote");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    // NOTHING STANDING: both refuse in words and publish nothing.
    b.r.key(input::scan::kP, input::mod::kShift);
    CHECK(b.text().find("nothing to promote") != std::string::npos);
    b.r.key(input::scan::kR, input::mod::kShift);
    CHECK(b.text().find("nothing to revert") != std::string::npos);
    CHECK(b.tool->promotes.empty());
    CHECK(b.tool->reverts.empty());

    // A REALIZED IMAGE IS STANDING: one offer each, naming the artifact the tool named.
    b.tool->next.artifact = "zengine-snake";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    b.r.key(input::scan::kP, input::mod::kShift);
    REQUIRE(b.tool->promotes.size() == 1);
    CHECK(b.tool->promotes[0] == "zengine-snake");
    b.r.key(input::scan::kR, input::mod::kShift);
    REQUIRE(b.tool->reverts.size() == 1);
    CHECK(b.tool->reverts[0] == "zengine-snake");
    // ...and the revert's sentence says what it leaves alone: the source the weaver saved.
    CHECK(b.text().find("saved source unchanged") != std::string::npos);
}

/// THE NOTICE ROW ITSELF -- the first row the pane says -- begins with `words`. The realize row
/// carries the owner's words too, so a search of the whole text cannot tell the two apart.
bool notice_says(BuilderRig& b, const std::string& words) {
    const std::vector<std::string> rows = b.shown();
    return !rows.empty() && rows[0].rfind(words, 0) == 0;
}

TEST_CASE("a build-and-load is said twice: the build's ending, then the load's answer") {
    BuilderRig b("bld-two-notices");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open(240, 60);
    b.r.key(input::scan::kB, input::mod::kShift);
    REQUIRE(b.text().find("load after build: on") != std::string::npos);

    SUBCASE("the gesture's sentence while it builds, the build's ending, then the load's") {
        b.letter(input::scan::kB, "b");
        REQUIRE(b.tool->realize_asked.size() == 1);
        REQUIRE(b.tool->realize_asked[0]);
        CHECK_MESSAGE(notice_says(b, "asked the Builder for `snake` and to realize it"), b.text());

        b.tool->next.op = 1;
        b.tool->next.artifact = "zengine-snake";
        b.tool->next.outcome = bld::outcome::kSucceeded;
        b.tool->next.detail = "built zengine-snake";
        b.tool->next.realization = bld::realization::kOffered;
        b.tool->next.realized_detail = "offered to the project";
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "succeeded `snake` -> zengine-snake -- built"), b.text());

        b.tool->next.realization = bld::realization::kRealized;
        b.tool->next.realized_detail = "reloaded in place";
        b.tool->next.default_image = false;
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "realize: realized, NOT DEFAULT (promote makes it the file a "
                                     "restart loads) -- reloaded in place"),
                      b.text());

        // THE LOAD WAS SAID ONCE: a revert's answer about the same image is the revert's row, and
        // the revert keeps its own sentence.
        b.r.key(input::scan::kR, input::mod::kShift);
        REQUIRE(b.tool->reverts.size() == 1);
        b.tool->next.realized_detail = "reverted: the previous image runs";
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "asked to revert `zengine-snake`"), b.text());
    }

    SUBCASE("a build that failed is one notice, its refused load said by the build's ending") {
        b.letter(input::scan::kB, "b");
        b.tool->next.op = 1;
        b.tool->next.outcome = bld::outcome::kFailed;
        b.tool->next.status = 2;
        b.tool->next.detail = "error: snake.cpp:3";
        b.tool->next.realization = bld::realization::kRefused;
        b.tool->next.realized_detail = "the build failed, so nothing was offered to the project";
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "FAILED `snake` -- error: snake.cpp:3"), b.text());
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "FAILED `snake`"), b.text());
    }

    SUBCASE("a build-and-load the tool refuses while another build runs leaves no load waiting") {
        b.tool->next.recipe = "snake";
        b.tool->next.builds = 1;
        b.tool->next.op = 7;
        b.tool->next.outcome = bld::outcome::kRunning;
        b.tool->next.realization = bld::realization::kNotAsked;
        b.tool->busy = true;
        b.tool_says();
        b.letter(input::scan::kB, "b");
        REQUIRE(b.tool->realize_asked.size() == 1);
        CHECK(b.tool->builds == 0);
        // The running plain build ends; its picture names no load to come.
        b.tool->busy = false;
        b.tool->next.artifact = "zengine-snake";
        b.tool->next.outcome = bld::outcome::kSucceeded;
        b.tool->next.detail = "built zengine-snake";
        b.tool_says();
        REQUIRE_MESSAGE(notice_says(b, "succeeded `snake`"), b.text());
        // A load the tool later answers for op #7 was asked by nobody here.
        b.tool->next.realization = bld::realization::kRealized;
        b.tool->next.realized_detail = "reloaded in place";
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "succeeded `snake`"), b.text());
    }

    SUBCASE("a later operation's load is never this one's") {
        b.letter(input::scan::kB, "b");
        b.tool->next.op = 1;
        b.tool->next.artifact = "zengine-snake";
        b.tool->next.outcome = bld::outcome::kSucceeded;
        b.tool->next.detail = "built zengine-snake";
        b.tool->next.realization = bld::realization::kOffered;
        b.tool_says();
        REQUIRE_MESSAGE(notice_says(b, "succeeded `snake`"), b.text());
        // ANOTHER ASKER'S BUILD-AND-LOAD is taken before this one's load is answered: op #1's offer
        // will not be answered now, and op #2's answer is not this pane's.
        b.tool->next.op = 2;
        b.tool->next.outcome = bld::outcome::kRunning;
        b.tool->next.realization = bld::realization::kAsked;
        b.tool_says();
        b.tool->next.outcome = bld::outcome::kSucceeded;
        b.tool->next.realization = bld::realization::kRealized;
        b.tool->next.realized_detail = "reloaded in place";
        b.tool_says();
        CHECK_MESSAGE(notice_says(b, "succeeded `snake`"), b.text());
    }
}

TEST_CASE("a build-and-load refused at dispatch is said with the office and Loom's reason, and "
          "nothing stays armed") {
    // THE OFFICE GOES BEFORE DELIVERY: Workshop approves the build, the ask is queued to the
    // tool's office with a valid ticket, and the office is gone when Loom would deliver it.
    BuilderRig b("bld-build-refused");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open(240, 60);
    b.r.key(input::scan::kB, input::mod::kShift);
    REQUIRE(b.text().find("load after build: on") != std::string::npos);
    const std::string bytes = b.r.bus.snapshot_bytes(b.tool_id);
    b.r.bus.kill(b.tool_id);
    b.letter(input::scan::kB, "b");
    CHECK_MESSAGE(notice_says(b, "`snake` was not built or loaded -- the ask could not reach "
                                 "zengine.builder (TargetUnavailable)"),
                  b.text());
    CHECK_MESSAGE(b.text().find("waiting for it to start") == std::string::npos, b.text());

    // THE OFFICE COMES BACK, and a load it then answers was asked by nobody here.
    REQUIRE(b.r.bus.swap_state(b.tool_id, bytes).revived);
    CHECK(b.tool->asked.empty());
    b.tool->next.recipe = "snake";
    b.tool->next.op = 3;
    b.tool->next.artifact = "zengine-snake";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool->next.realized_detail = "reloaded in place";
    b.tool_says();
    CHECK_MESSAGE(notice_says(b, "`snake` was not built or loaded"), b.text());
}

// ============================================================================
// BLD-WEAVE — the frontier, asked for rather than read
// ============================================================================

TEST_CASE("the frontier row comes from the host's read-only door") {
    // THE FRONTIER CROSSES AS A SENTENCE. The realization owner is in the host's `main` and the
    // pane is a loaded image, so what it has is a picture it asked for -- and knowing what a
    // project waits on is not permission to perform a row.
    BuilderRig b("bld-frontier");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.frontier.waiting = true;
    b.frontier.artifact = "zengine-snake";
    b.frontier.blocked = 3;
    b.open();

    CHECK(b.text().find("waiting zengine-snake (snake, blocks 3)") != std::string::npos);

    // NOT WAITING IS THE ABSENCE OF THE ROW, not a row saying nothing is blocked.
    b.frontier = ProjectFrontier{};
    b.tool_says(); // any settled status re-asks the frontier
    CHECK(b.text().find("waiting ") == std::string::npos);
}

TEST_CASE("`f` builds and realizes the one recipe that makes the frontier") {
    BuilderRig b("bld-f");
    b.tool->catalog = catalog_of({{"one", "a"}, {"snake", "zengine-snake"}});
    b.frontier.waiting = true;
    b.frontier.artifact = "zengine-snake";
    b.open();

    b.letter(input::scan::kF, "f");
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    CHECK(b.tool->realize_asked[0] == true);
    // THE SELECTION MOVED WITH THE GESTURE, VISIBLY: the row now names what was asked for.
    CHECK(b.text().find("snake -> zengine-snake  (2/2)") != std::string::npos);
}

TEST_CASE("`f` refuses in words, and never chooses between recipes") {
    BuilderRig b("bld-f-refuse");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open();

    // NOTHING WAITING.
    b.letter(input::scan::kF, "f");
    CHECK(b.text().find("not waiting on any artifact") != std::string::npos);
    CHECK(b.tool->asked.empty());

    // WAITING ON SOMETHING NO RECIPE PRODUCES.
    b.frontier.waiting = true;
    b.frontier.artifact = "zengine-nothing";
    b.letter(input::scan::kF, "f");
    CHECK(b.text().find("no authored recipe produces") != std::string::npos);
    CHECK(b.tool->asked.empty());

    // SEVERAL PRODUCE IT: named and counted, and the pick left to the weaver.
    b.frontier.artifact = "a";
    b.letter(input::scan::kF, "f");
    CHECK(b.text().find("2 recipes produce") != std::string::npos);
    CHECK(b.tool->asked.empty());

    // ...AND AFTER AN EXPLICIT PICK THAT PRODUCES IT, the gesture spends it.
    b.letter(input::scan::kC, "c"); // an explicit pick: `two`, which produces `a`
    b.letter(input::scan::kF, "f");
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "two");
}

TEST_CASE("the recipe `f` took as the one producer carries no pick of another recipe") {
    BuilderRig b("bld-f-took");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.frontier.waiting = true;
    b.frontier.artifact = "a";
    b.open();

    b.letter(input::scan::kC, "c"); // an explicit pick: `two`, which does not produce `a`
    REQUIRE(b.text().find("two -> b  (2/2)") != std::string::npos);
    // ONE PRODUCER: `f` chooses it, visibly, and builds it -- the gesture's choice, not the
    // weaver's.
    b.letter(input::scan::kF, "f");
    REQUIRE(b.tool->asked.size() == 1);
    REQUIRE(b.tool->asked[0] == "one");
    REQUIRE(b.text().find("one -> a  (1/2)") != std::string::npos);

    // A SECOND PRODUCER ARRIVES: the pick still names `two`, so nothing picked stands between
    // them.
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "a"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    b.letter(input::scan::kF, "f");
    CHECK(b.tool->asked.size() == 1);
    CHECK(b.text().find("2 recipes produce `a` (`one`, `three`)") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — the plan row, authored through the acting door
// ============================================================================

TEST_CASE("`o` asks for a role in the pane's own room, and authors it") {
    // THE ROLE PROMPT IS A LINE INSIDE THE PANE, as Files' is: a weave has one room and no
    // keyboard context of Workshop's, so the line lives in its own rows and its two gestures
    // are two of its own declared actions.
    BuilderRig b("bld-load");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    b.letter(input::scan::kO, "o");
    CHECK(b.text().find("role for zengine-snake") != std::string::npos);
    // ...AND WHILE THE LINE HAS THE KEYBOARD THE PANE DECLARES ITS THREE MODE ROWS AND NO
    // MORE, so every other key reaches it as an ordinary keystroke for the line to consume, as
    // in Files (WL-FILES-16). None of the build verbs is among them.
    const RuntimePane* seat = b.row();
    REQUIRE(seat != nullptr);
    REQUIRE(seat->actions.size() == kRoleLineIds.size());
    for (std::size_t i = 0; i < kRoleLineIds.size(); ++i) {
        CHECK(seat->actions[i].id == kRoleLineIds[i]);
    }

    // AN EMPTY ROLE IS REFUSED IN THE PLAN'S OWN WORDS, and nothing is written.
    b.r.key(input::scan::kReturn);
    CHECK(b.text().find("needs a role") != std::string::npos);
    CHECK(b.authored.empty());

    // A TYPED ROLE CROSSES TO THE ONE WRITER, and the answer is said in the pane's row.
    b.r.text("zengine.oven");
    b.r.key(input::scan::kReturn);
    REQUIRE(b.authored.size() == 1);
    CHECK(b.authored[0].stem == "zengine-snake");
    CHECK(b.authored[0].role == "zengine.oven");
    CHECK(b.authored[0].recipe == "snake");
    CHECK(b.text().find("loaded `zengine-snake` as zengine.oven") != std::string::npos);
    // ...AND THE ROWS COME BACK, because the mode closed and the declaration was replaced.
    REQUIRE(b.row()->actions.size() == kBrowsingRows);
}

TEST_CASE("Escape abandons the line, and nothing is written") {
    BuilderRig b("bld-load-cancel");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    b.letter(input::scan::kO, "o");
    b.r.text("half-typed");
    b.r.key(input::scan::kEscape);
    CHECK(b.text().find("nothing was loaded and nothing was written") != std::string::npos);
    CHECK(b.authored.empty());
    REQUIRE(b.row()->actions.size() == kBrowsingRows);
}

TEST_CASE("an artifact the plan already names is refused before the line") {
    // THE READ AND THE ACT ARE TWO OFFICES, and this is the read: the pane asks
    // `zengine.project` whether the plan names the stem and opens the line only if it does
    // not, so a weaver never types a role for a row that cannot be written.
    BuilderRig b("bld-load-dup");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.plan_rows.push_back("zengine-snake");
    b.open();

    b.letter(input::scan::kO, "o");
    CHECK(b.text().find("already in this project's plan") != std::string::npos);
    CHECK(b.text().find("role for") == std::string::npos);
    CHECK(b.authored.empty());
}

TEST_CASE("a refused row is said in the owner's own words") {
    BuilderRig b("bld-load-refused");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.next_append.accepted = false;
    b.next_append.refusal = "that role is already held";
    b.open();

    b.letter(input::scan::kO, "o");
    b.r.text("zengine.oven");
    b.r.key(input::scan::kReturn);
    REQUIRE(b.authored.size() == 1);
    CHECK(b.text().find("not loaded: that role is already held") != std::string::npos);
}

TEST_CASE("a row whose product is built finishes with the button's act") {
    BuilderRig b("bld-load-built");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.next_append.frontier = true;
    b.next_append.product = "/build/zengine-snake.so";
    b.next_append.detail = "waiting";
    b.open();

    b.letter(input::scan::kO, "o");
    b.r.text("zengine.oven");
    b.r.key(input::scan::kReturn);
    // THE INTENT FINISHES: the finished build's recipe asked for again with the second
    // intention aboard, to the same office, under the same grant. No new route.
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    CHECK(b.tool->realize_asked[0] == true);
    // ...AND THE WEAVER IS TOLD, in the sentence a narrow pane can actually carry: the pane's
    // room is one stack slot wide, so its notices front-load their meaning the way a path
    // does not (WL-PROJ-10's rule, one artifact over) -- the act first, the details after.
    CHECK(b.text().find("loading `zengine-snake` now") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — the Editor door, reached by a recipe's name
// ============================================================================

TEST_CASE("`e` opens the chosen recipe's source, resolved by the host") {
    // THE PANE NAMES A RECIPE AND NEVER A PATH. `RecipeSummary` is `{recipe, artifact}` on
    // purpose, so the sentence that crosses is the recipe's own name and the host's read-only
    // project office resolves it against the catalog IT owns -- a pane that could spell the
    // path would already have been handed the build procedure. The pane then carries the one
    // path the door named to the Editor's own door, and the Editor asks to be shown.
    BuilderRig b("bld-edit");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    const std::string src = (b.root / "snake.cpp").generic_string();
    std::ofstream out(src, std::ios::binary | std::ios::trunc);
    out << "int main() {}\n";
    out.close();
    b.next_source.known = true;
    b.next_source.kind = "single_source";
    b.next_source.source = src;
    b.open(160, 48, /*with_editor=*/true);

    b.letter(input::scan::kE, "e");
    const std::int64_t editor = b.editor_kind();
    REQUIRE(b.r.session().panes.has(editor));
    CHECK(b.r.session().panes.keyboard == editor);
    CHECK(b.editor_status().rfind("saved L1:C1/2", 0) == 0);
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    const std::vector<std::string> rows = pane_rows(b.r, editor);
    CHECK(std::find(rows.begin(), rows.end(), "int main() {}") != rows.end());
}

TEST_CASE("a refusal from either door is said in the pane's own row") {
    // THE FIRST DOOR'S REFUSAL: a kind with no single source, in the recipe file's own words.
    BuilderRig b("bld-edit-refuse");
    b.tool->catalog = catalog_of({{"block", "zen-block"}});
    b.next_source.known = true;
    b.next_source.kind = "cmake_target"; // no single source to edit
    b.open(160, 48, /*with_editor=*/true);

    b.letter(input::scan::kE, "e");
    CHECK_FALSE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.text().find("names no source file or editing entry") != std::string::npos);

    // THE SECOND DOOR'S REFUSAL: a file that is not there, in the Editor's own words.
    BuilderRig c("bld-edit-missing");
    c.tool->catalog = catalog_of({{"gone", "zen-gone"}});
    c.next_source.known = true;
    c.next_source.kind = "single_source";
    c.next_source.source = (c.root / "absent.cpp").generic_string();
    c.open(160, 48, /*with_editor=*/true);
    c.letter(input::scan::kE, "e");
    CHECK_FALSE(c.r.session().panes.has(c.editor_kind()));
    CHECK(c.text().find("cannot read") != std::string::npos); // the reader's own words

    // ...AND A HOST WITH NO EDITOR IN THE ROOM: the second ask reaches the opening manager,
    // which finds no Editor to bind and REFUSES IN WORDS at once -- an immediate refusal,
    // never silence and never a standalone mode inferred from it -- and the pane says those
    // words (WL-OPEN-07).
    BuilderRig d("bld-edit-nobody");
    d.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    d.next_source.known = true;
    d.next_source.kind = "single_source";
    d.next_source.source = (d.root / "snake.cpp").generic_string();
    d.open();
    const std::string before = d.text();
    d.letter(input::scan::kE, "e");
    CHECK(d.text() != before);
    CHECK(d.text().find("no Editor") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — what the pane keeps, and what it re-asks
// ============================================================================

TEST_CASE("closing the pane forgets its copy; the TOOL keeps its own count") {
    // The pane's copy of somebody else's facts is a member and not state (WL-PROJ-12), and
    // Workshop stops granting it a room when the pane is removed -- so reopening asks again and
    // is answered with the tool's own running total.
    BuilderRig b("bld-forget");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    b.letter(input::scan::kB, "b");
    const std::int64_t asked_once = b.tool->described;

    b.unfocus();
    b.r.pick(builder_ref()); // the same door removes it
    REQUIRE_FALSE(b.r.session().panes.has(b.kind));
    b.r.pick(builder_ref());
    b.focus();
    CHECK(b.tool->described > asked_once);
    // ...AND THE TOOL'S OWN COUNTER IS WHAT COMES BACK: the pane held no copy across the close,
    // so it shows the tool's running total, which a pane owning the state could not produce. The
    // count is read from the tool, since `asks N ever` is the first row this pane's composition
    // gives up under a constrained budget.
    CHECK(b.tool->builds == 1);
    CHECK(b.text().find("recipe   snake") != std::string::npos);
}

TEST_CASE("the office the migration writes is the one this pane holds") {
    // THE TWO SPELLINGS, CHECKED AGAINST EACH OTHER. `pane_migration.hpp` names the new
    // office as a literal because the host does not link this weave; this is the seam where
    // a divergence between the two would actually be caught.
    CHECK(std::string(pane_migration::kBuilderProvider) == std::string(pane::kBuilderPaneRole));
    CHECK(std::string(pane_migration::kBuilderPane) == std::string(pane::kBuilderPane));
    // ...AND A SAVED SETUP NAMING THE BUILT-IN'S REFERENCE RESOLVES TO THIS PANE AFTER THE
    // CONVERSION.
    BuilderRig b("bld-mig");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    Setup s;
    s.name = "Yesterday";
    REQUIRE(add_pane(s, PaneRef{pane_migration::kRetiredBuilderProvider,
                                pane_migration::kBuilderPane}));
    const pane_migration::Converted moved = pane_migration::convert_retired_panes(s);
    CHECK(pane_migration::held_count(moved, pane_migration::kBuilderProvider) == 1);
    CHECK(pane_migration::held_count(moved, pane_migration::kFilesProvider) == 0);
    CHECK(pane_migration::held_count(moved, pane_migration::kInfoProvider) == 0);
    REQUIRE(s.panes.size() == 1);
    CHECK(s.panes[0].ref == builder_ref());
    CHECK(resolve_pane(s.panes[0].ref, b.r.session().panes).value_or(kNoPaneKind) == b.kind);
}

TEST_CASE("the package links no kernel and mounts nothing") {
    // THE BUILD GRAPH, READ AS TEXT. This pane asks offices and hears answers; it never touches
    // the control door or the Manager, and the five names a presentation may not spell are as
    // forbidden in its own image as they are in this host's sources.
    const std::string cmake = file_source(BUILDER_PANE_CMAKE);
    CHECK(cmake.find("loom::kernel") != std::string::npos); // it ASKS whether one exists...
    CHECK(cmake.find("target_link_libraries(zengine-builder-pane PRIVATE loom::kernel") ==
          std::string::npos); // ...and does not link it
    const std::string source = file_source(BUILDER_PANE_SOURCE);
    for (const char* forbidden : {"PlanExecutor", "load_execute", "OfferArtifact", "RunBuild",
                                  "kBuildRunnerRole", "HostContext", "Session&"}) {
        CHECK_MESSAGE(source.find(forbidden) == std::string::npos, "the pane names '",
                      forbidden, "'");
    }
}

TEST_CASE("a weaver's authored override for a retired Workshop id keeps working") {
    // A WEAVER'S MOVED KEYS HOLD, END TO END: `builder.build` is the pane's action id and
    // `authoring.commit` one of its two mode rows, so a weaver who moved either finds the key where
    // they put it. The file is read BEFORE the pane exists, the order that matters: both rows are
    // kept as unknown at load (WL-KEY-06), and the pane's declaration makes them known, the
    // weaver's gesture then applied to it (WL-KEY-15).
    TempDir keys("bld-override");
    const std::string path = keys.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"builder.build", "ctrl+u"},
                                                         {"authoring.commit", "ctrl+j"}}));

    BuilderRig b("bld-override");
    b.r.host.keymap_path = path;
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    // THE MOVED GESTURE BUILDS, and the default does not -- which is what "moved" means.
    b.r.key(input::scan::kU, input::mod::kCtrl);
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    b.letter(input::scan::kB, "b");
    CHECK(b.tool->asked.size() == 1); // `b` is an ordinary character here now

    // ...AND SO DOES THE ROLE LINE'S COMMIT, under the id a keymap file names it by.
    b.letter(input::scan::kO, "o");
    b.r.text("zengine.oven");
    b.r.key(input::scan::kReturn); // the default, which the weaver moved away from
    CHECK(b.authored.empty());
    b.r.key(input::scan::kJ, input::mod::kCtrl);
    REQUIRE(b.authored.size() == 1);
    CHECK(b.authored[0].role == "zengine.oven");
}


namespace {
/// A source file on disk, for the cases that open one.
inline void put_source(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

struct HeldLookupState {
    std::int64_t asked = 0;
    ZEN_SHAPE(HeldLookupState, 1, ZEN_FIELD(asked));
};

/// A PROJECT OFFICE THAT HOLDS ITS ANSWER TO A SOURCE LOOKUP until a case releases it (test
/// instrumentation). The real door answers inside the delivery that asked, so while it holds
/// the office no forgery can be delivered between the lookup and its answer; this office keeps
/// the lookup genuinely outstanding for exactly as long as a case needs, and then answers as
/// the real door would, through the answer right Loom kept for the ask.
class HeldLookupOffice : public loom::WeaveBase<HeldLookupOffice, HeldLookupState,
                                                loom::Accept<RecipeSourceRequested, SeatDo>,
                                                loom::Emit<RecipeSourceSaid>> {
public:
    RecipeSourceSaid answer_with;
    loom::DeferredAnswer held;

    void on(const RecipeSourceRequested&, loom::Mail& mail) {
        ++state_.asked;
        held = mail.defer_answer();
    }
    /// THE RELEASE: the held answer, now.
    void on(const SeatDo&, loom::Mail& mail) {
        if (held.valid()) {
            (void)loom::answer_deferred(held, mail, answer_with);
            held = loom::DeferredAnswer{};
        }
    }
};
} // namespace

// ============================================================================
// BLD-WEAVE -- a refusal at enqueue, at dispatch, and a forgery are three facts (WL-OPEN-07)
// ============================================================================

TEST_CASE("a lookup queued to a project office nobody holds and an open queued to an opening "
          "office nobody holds are each refused at dispatch by that attempt, in words, and a fresh "
          "e takes once each office is present") {
    // THE FIRST DOOR REACHES NOBODY: no project office is held. The pane declares
    // `RecipeSourceRequested` in its `Emit<...>`, and a declared shape is registered by its
    // emitter at load, so the lookup is queued to the unheld office, refused at dispatch, and the
    // pane says in words that the lookup failed. Its branch for a ticket that is not valid is not
    // reachable through a declared shape and stays source-traced (`edit_source`,
    // builder/builder-pane/pane.cpp).
    BuilderRig a("bld-lookup-refused");
    a.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    a.open(160, 48, /*with_editor=*/true, /*with_manager=*/true, /*with_project_door=*/false);
    REQUIRE(a.r.bus.resolve_schema(RecipeSourceRequested::zen_name,
                                   RecipeSourceRequested::zen_version) != nullptr);
    a.letter(input::scan::kE, "e");
    CHECK_MESSAGE(a.text().find("`snake`") != std::string::npos, a.text());
    CHECK_MESSAGE(a.text().find("could not be looked up") != std::string::npos, a.text());
    CHECK_MESSAGE(a.text().find("it could not reach") != std::string::npos, a.text());
    CHECK_MESSAGE(a.text().find("NoSuchTarget") != std::string::npos, a.text());
    CHECK_MESSAGE(a.text().find("nothing was queued") == std::string::npos, a.text());
    CHECK_FALSE(a.r.session().panes.has(a.editor_kind()));
    CHECK(a.r.opening->state().op == 0); // the manager was never asked
    // ...AND A FRESH `e` TAKES once the office is held: the same rig, the door mounted late.
    a.mount_doors(true);
    a.letter(input::scan::kE, "e");
    CHECK_MESSAGE(a.text().find("could not be looked up") == std::string::npos, a.text());

    // THE SECOND DOOR REACHES NOBODY: the project answers, the open is queued to an opening
    // office nobody holds and refused at dispatch, and the pane names the open as the stage
    // that failed; nothing moves on the desk.
    BuilderRig b("bld-open-refused");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.next_source.known = true;
    b.next_source.kind = "single_source";
    b.next_source.source = (b.root / "snake.cpp").generic_string();
    put_source(b.root / "snake.cpp", "int main() {}\n");
    b.open(160, 48, /*with_editor=*/true, /*with_manager=*/false);
    b.letter(input::scan::kE, "e");
    CHECK_MESSAGE(b.text().find("`snake`") != std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("was not opened") != std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("could not reach") != std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("NoSuchTarget") != std::string::npos, b.text());
    CHECK_FALSE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.r.session().panes.keyboard == b.kind);
    // ...AND A FRESH `e` TAKES once the office is held.
    b.r.mount_opening();
    b.letter(input::scan::kE, "e");
    REQUIRE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.r.session().panes.keyboard == b.editor_kind());
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
}

TEST_CASE("`o` and the frontier action asked of a project office nobody holds are each refused at "
          "dispatch, in words") {
    // BOTH ASKS ARE QUEUED WITH A VALID TICKET to an office nobody holds, and Loom refuses each by
    // that attempt. The frontier the pane asks for its own picture at the grant is refused the same
    // way, and it is no act of the weaver's: nothing is said for it.
    BuilderRig a("bld-asks-refused");
    a.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    a.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/false);
    CHECK_MESSAGE(a.text().find("could not reach") == std::string::npos, a.text());

    a.letter(input::scan::kO, "o");
    CHECK_MESSAGE(a.text().find("`snake`: nothing was added to the plan -- the ask could not reach "
                                "zengine.project (NoSuchTarget)") != std::string::npos,
                  a.text());
    CHECK_MESSAGE(a.text().find("role for") == std::string::npos, a.text());

    a.letter(input::scan::kF, "f");
    CHECK_MESSAGE(a.text().find("nothing was asked for -- the ask could not reach zengine.project "
                                "(NoSuchTarget)") != std::string::npos,
                  a.text());
    CHECK(a.tool->asked.empty());

    // ...AND A FRESH `o` TAKES once the office is held.
    a.mount_doors(true);
    a.letter(input::scan::kO, "o");
    CHECK_MESSAGE(a.text().find("role for zengine-snake") != std::string::npos, a.text());
}

TEST_CASE("a role line's row refused at dispatch by the plan office is said, and nothing is written "
          "or built") {
    // THE OFFICE GOES BEFORE DELIVERY: Workshop approves the commit, the row is queued to the plan
    // office with a valid ticket, and the office is gone when Loom would deliver it.
    BuilderRig b("bld-row-refused");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    b.letter(input::scan::kO, "o");
    REQUIRE_MESSAGE(b.text().find("role for zengine-snake") != std::string::npos, b.text());
    b.r.text("zengine.oven");
    REQUIRE(b.plan_id.valid());
    b.r.bus.kill(b.plan_id);
    b.r.key(input::scan::kReturn);
    CHECK_MESSAGE(b.text().find("`zengine-snake`: nothing was loaded and nothing was written -- the "
                                "row could not reach zengine.plan (TargetUnavailable)") !=
                      std::string::npos,
                  b.text());
    CHECK(b.authored.empty());
    CHECK(b.tool->asked.empty());
}

TEST_CASE("a lookup queued to the project office and refused at dispatch -- the office gone before "
          "delivery -- is said by that exact attempt at the lookup stage, opens nothing, and a "
          "fresh e takes once the office is back") {
    // LOOM'S LATER WORD, NOT THE IMMEDIATE ONE. The office is held and the shape declared when `e`
    // asks, so the lookup is QUEUED with a valid ticket; the office is then killed between the
    // turn that queued it and the turn that would deliver it, so Loom refuses that attempt at
    // dispatch and tells the pane by it (`zen.DispatchRefused`). The enqueue refusal is the case
    // above and a forged notice the case below; none of the three proves another.
    BuilderRig b("bld-lookup-dispatch-refused");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.next_source.known = true;
    b.next_source.kind = "single_source";
    b.next_source.source = (b.root / "snake.cpp").generic_string();
    put_source(b.root / "snake.cpp", "int main() {}\n");
    b.open(160, 48, /*with_editor=*/true);
    const loom::WeaveId pane_id = b.r.kernel.weave_id(pane::kBuilderPaneStem);
    REQUIRE(pane_id.value != 0);
    REQUIRE(b.project_id.valid());
    REQUIRE(b.r.bus.resolve_schema(RecipeSourceRequested::zen_name,
                                   RecipeSourceRequested::zen_version) != nullptr);
    // WHAT THE BUS SAYS, read off its tap: where the turn ends, the lookup's refusal, the
    // notice the pane is handed, and whether the pane ever asks to open.
    loom::Switchboard& bus = b.r.bus;
    bool stopped = false;
    std::uint64_t action = 0;
    std::uint64_t refused_seq = 0;
    std::uint64_t refused_parent = 0;
    loom::RefusalReason refused_reason = loom::RefusalReason::None;
    std::string refused_role;
    std::uint64_t notice_attempt = 0;
    std::uint64_t notice_parent = 0;
    bool notice_from_bus = false;
    int notices = 0;
    int opens_asked = 0;
    const loom::ObserverId tap = bus.add_observer([&](const loom::BusEvent& ev) {
        if (!stopped && ev.kind == loom::EventKind::Delivered && ev.target == pane_id &&
            ev.schema_name == PaneActionRequested::zen_name) {
            // THE TURN ENDS WHERE THE PANE HAS HEARD `e`: its lookup is queued, not delivered.
            stopped = true;
            action = ev.seq;
            bus.stop();
        }
        if (ev.kind == loom::EventKind::Refused && ev.sender == pane_id &&
            ev.schema_name == RecipeSourceRequested::zen_name) {
            refused_seq = ev.seq;
            refused_parent = ev.dispatch_parent;
            refused_reason = ev.refusal.reason;
            refused_role = ev.addressed_role;
        }
        if (ev.kind == loom::EventKind::Delivered && ev.target == pane_id &&
            ev.schema_name == loom::DispatchRefused::zen_name && ev.payload != nullptr) {
            ++notices;
            notice_attempt = loom::from_value<loom::DispatchRefused>(*ev.payload).refused_attempt().seq;
            notice_parent = ev.dispatch_parent;
            notice_from_bus = !ev.sender.valid();
        }
        if (ev.kind == loom::EventKind::Delivered && ev.sender == pane_id &&
            ev.schema_name == OpenSourceRequested::zen_name) {
            ++opens_asked;
        }
    });
    b.enqueue_letter(input::scan::kE, "e");
    for (int turns = 0; turns < 8 && !stopped; ++turns) {
        (void)bus.pump_pending();
    }
    REQUIRE(stopped);
    // THE LOOKUP IS QUEUED AND NOTHING HAS REFUSED IT: no seam refusal, no dispatch yet.
    CHECK(bus.pending() > 0);
    CHECK(refused_seq == 0);
    CHECK(b.text().find("could not be looked up") == std::string::npos);
    // THE OFFICE GOES BEFORE ITS DELIVERY.
    bus.kill(b.project_id);
    bus.drain_until_idle();
    bus.remove_observer(tap);
    // LOOM'S WORD: that queued attempt, authored in the delivery the turn ended on, refused at
    // dispatch for the office it was addressed to -- a reason no enqueue can give.
    REQUIRE(refused_seq != 0);
    CHECK(refused_seq > action);
    CHECK(refused_parent == action);
    CHECK(refused_reason == loom::RefusalReason::TargetUnavailable);
    CHECK(refused_role == kProjectRole);
    CHECK(bus.outcome(loom::Ticket{refused_seq}).disposition == loom::Disposition::Refused);
    // ...HANDED TO THE PANE AS LOOM'S OWN NOTICE, BY THAT EXACT ATTEMPT.
    CHECK(notices == 1);
    CHECK(notice_from_bus);
    CHECK(notice_parent == refused_seq);
    CHECK(notice_attempt == refused_seq);
    // THE WEAVER READS THE RECIPE, THE STAGE, THE OFFICE AND THE REASON -- not the enqueue
    // refusal's words -- and nothing was opened.
    CHECK_MESSAGE(b.text().find("`snake`: the source could not be looked up -- it could not "
                                "reach zengine.project (TargetUnavailable)") != std::string::npos,
                  b.text());
    CHECK(b.text().find("nothing was queued") == std::string::npos);
    CHECK(opens_asked == 0);
    CHECK_FALSE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.r.opening->state().op == 0);
    CHECK(b.r.opening->state().committed == 0);
    CHECK(bus.resolve_schema(RecipeSourceRequested::zen_name,
                             RecipeSourceRequested::zen_version) != nullptr);
    // THE OFFICE COMES BACK -- revived in place, the same office -- and a fresh `e` takes.
    const std::string bytes = bus.snapshot_bytes(b.project_id);
    REQUIRE(bus.swap_state(b.project_id, bytes).revived);
    b.letter(input::scan::kE, "e");
    REQUIRE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.r.session().panes.keyboard == b.editor_kind());
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    // ...AND THE REFUSAL IT SPENT IS GONE FROM THE PANE'S PUBLISHED ROWS -- the rows Workshop
    // holds for it, which are the rows a weaver reads beside the opened source.
    CHECK_MESSAGE(b.text().find("could not be looked up") == std::string::npos, b.text());
}

TEST_CASE("the Builder's refusal leaves its published rows at the weaver's next e while that lookup is still unanswered, stays gone once the source opens, and a new refusal stands through a repaint until the act after it") {
    // A PRIVATE STATE CHANGE IS COMPLETE WHEN THE PUBLISHED PICTURE SAYS IT. The pane clears its
    // notice where the weaver acts; an act whose answer is still on its way publishes nothing of
    // its own, so the rows Workshop holds must be said again at that act -- not at the answer,
    // and never inside `say`, which would lose a new refusal on the first unrelated repaint.
    BuilderRig b("bld-notice-spent");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    const std::string src = (b.root / "snake.cpp").generic_string();
    put_source(b.root / "snake.cpp", "int main() {}\n");
    b.open(160, 48, /*with_editor=*/true, /*with_manager=*/true, /*with_project_door=*/false);
    // A STANDING REFUSAL: `e` with no project office, so nothing could be queued.
    b.letter(input::scan::kE, "e");
    REQUIRE_MESSAGE(b.text().find("could not be looked up") != std::string::npos, b.text());
    // ...AND A REPAINT THE WEAVER DID NOT MAKE KEEPS IT: the tool republishes, the pane says again.
    b.tool_says();
    CHECK_MESSAGE(b.text().find("could not be looked up") != std::string::npos, b.text());
    // THE OFFICE ARRIVES HOLDING ITS ANSWER, and the weaver presses `e` again.
    auto held = std::make_unique<HeldLookupOffice>();
    HeldLookupOffice* office = held.get();
    office->answer_with = RecipeSourceSaid{"snake", true, std::string(), src};
    loom::Grant say;
    say.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
    const loom::WeaveId office_id =
        b.r.bus.register_weave(std::move(held), std::move(say), std::string(kProjectRole));
    office->zen_set_self(office_id);
    const auto release = [&b, office_id] {
        (void)b.r.bus.send(office_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
        b.r.bus.drain_until_idle();
    };
    b.letter(input::scan::kE, "e");
    // THE LOOKUP IS UNANSWERED -- and the refusal it spent is already gone from the rows.
    REQUIRE(office->held.valid());
    CHECK_FALSE(b.r.session().panes.has(b.editor_kind()));
    CHECK_MESSAGE(b.text().find("could not be looked up") == std::string::npos, b.text());
    // THE ANSWER, released: the source opens, and the rows stay clean.
    release();
    REQUIRE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    CHECK_MESSAGE(b.text().find("could not be looked up") == std::string::npos, b.text());
    // A NEW REFUSAL, from the office's own words, stands through an unrelated repaint...
    b.focus();
    office->answer_with = RecipeSourceSaid{"snake", false, "`snake` names no file here", ""};
    b.letter(input::scan::kE, "e");
    REQUIRE(office->held.valid());
    release();
    REQUIRE_MESSAGE(b.text().find("`snake` names no file here") != std::string::npos, b.text());
    b.tool_says();
    CHECK_MESSAGE(b.text().find("`snake` names no file here") != std::string::npos, b.text());
    // ...until the weaver's next act, which says its own.
    b.letter(input::scan::kC, "c");
    CHECK_MESSAGE(b.text().find("`snake` names no file here") == std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("build recipe: snake") != std::string::npos, b.text());
}

TEST_CASE("a key the Builder's role line does not take is no act: the notice stands through a repaint, and a key it takes spends it") {
    // THE OTHER HALF OF "SPENT MEANS PUBLISHED": what is not an act spends nothing. The line
    // consumes its editing keys and refuses the rest; a refused key that cleared the notice
    // privately and said no rows would let the next unrelated repaint drop a sentence the weaver
    // had done nothing to.
    BuilderRig b("bld-notice-unspent");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    b.letter(input::scan::kO, "o");
    REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    b.r.key(input::scan::kDown); // a key the line has no meaning for
    b.tool_says();               // an unrelated repaint
    CHECK_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    b.r.key(input::scan::kLeft); // a key the line takes
    CHECK_MESSAGE(b.text().find("type the role it holds") == std::string::npos, b.text());
}

TEST_CASE("an id the Builder does not declare in the mode it is in is no act: an unknown one, a build id while the role line is open, and a commit resolved after the line closed leave the notice and the line standing through a new room, and a declared id through the same door acts") {
    // WHAT IS NOT AN ACT SPENDS NOTHING, FOR AN ID AS FOR A KEY: a pane that cleared its notice
    // before asking what the id meant in its mode would let an id nobody declared, or one that
    // raced the pane's own re-declaration, erase the line's instructions or the cancel's answer.
    BuilderRig b("bld-ids-unspent");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    const std::vector<std::string>& line_ids = kRoleLineIds;
    b.letter(input::scan::kO, "o");
    REQUIRE(b.declared() == line_ids);
    REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    const std::int64_t builds = b.tool->builds;
    const auto line_stands = [&b, &line_ids, builds] {
        CHECK_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
        CHECK(b.declared() == line_ids);
        CHECK(b.tool->builds == builds);
        CHECK(b.authored.empty());
    };

    // AN ID NOBODY DECLARED, SAID BY WORKSHOP'S OWN OFFICE.
    const PaneRig::OfficeAction unknown =
        b.r.workshop_action(pane::kBuilderPaneRole, pane::kBuilderPane, "builder.no-such-action");
    REQUIRE(unknown.authored);
    REQUIRE(unknown.delivered);
    CHECK(unknown.author == kWorkshopProvider);
    line_stands();
    b.regrant();
    line_stands();

    // AN ID THE PANE DECLARES WHEN BROWSING, DELIVERED WHILE THE LINE IS OPEN.
    const PaneRig::OfficeAction build =
        b.r.workshop_action(pane::kBuilderPaneRole, pane::kBuilderPane, pane::kActionBuild);
    REQUIRE(build.delivered);
    line_stands();
    b.regrant();
    line_stands();

    // A DECLARED ID THROUGH THE SAME DOOR IS AN ACT: the line closes and its own answer stands.
    const PaneRig::OfficeAction cancel =
        b.r.workshop_action(pane::kBuilderPaneRole, pane::kBuilderPane, pane::kActionCancel);
    REQUIRE(cancel.delivered);
    CHECK(b.text().find("type the role it holds") == std::string::npos);
    CHECK(b.text().find("nothing was loaded and nothing was written") != std::string::npos);
    CHECK(b.declared().size() == kBrowsingRows);

    // ESCAPE AND RETURN IN ONE POLL: the cancel, then a commit Workshop resolved against the
    // line's rows, arriving after the line closed. The cancel's answer stands.
    b.letter(input::scan::kO, "o");
    REQUIRE(b.declared() == line_ids);
    b.enqueue_key(input::scan::kEscape);
    b.enqueue_key(input::scan::kReturn);
    b.settle();
    CHECK(b.declared().size() == kBrowsingRows);
    CHECK_MESSAGE(b.text().find("nothing was loaded and nothing was written") != std::string::npos,
                  b.text());
    b.regrant();
    CHECK_MESSAGE(b.text().find("nothing was loaded and nothing was written") != std::string::npos,
                  b.text());
    CHECK(b.authored.empty());
    CHECK(b.tool->builds == builds);
}

TEST_CASE("a forged refusal naming the pane's own live attempt settles nothing at either stage, "
          "and the open completes") {
    // THE PROVENANCE IS THE FACT. While each ask is outstanding, a stranger says
    // `zen.DispatchRefused` with the RIGHT attempt number, read off the bus's tap, and the pane
    // settles nothing on it: the source opens. The real project door answers inside the delivery
    // that asked, which would put the forgery behind the answer, so the lookup's office holds its
    // answer until the forgery has reached the pane, and the order the pane heard things in is
    // asserted: a forgery that stops arriving in time turns this case red.
    BuilderRig b("bld-forged");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    const std::string src = (b.root / "snake.cpp").generic_string();
    put_source(b.root / "snake.cpp", "int main() {}\n");
    b.open(160, 48, /*with_editor=*/true, /*with_manager=*/true, /*with_project_door=*/false);
    auto held = std::make_unique<HeldLookupOffice>();
    HeldLookupOffice* office = held.get();
    office->answer_with = RecipeSourceSaid{"snake", true, std::string(), src};
    loom::Grant say;
    say.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
    const loom::WeaveId office_id =
        b.r.bus.register_weave(std::move(held), std::move(say), std::string(kProjectRole));
    office->zen_set_self(office_id);
    const loom::WeaveId pane_id = b.r.kernel.weave_id(pane::kBuilderPaneStem);
    REQUIRE(pane_id.value != 0);
    b.mount_stranger();
    std::uint64_t lookup = 0;
    std::uint64_t open = 0;
    std::vector<std::string> heard; ///< what reached the pane, in order
    const loom::WeaveId stranger_id = b.stranger_id;
    const loom::ObserverId tap = b.r.bus.add_observer(
        [&lookup, &open, &heard, pane_id, stranger_id](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            if (ev.sender == pane_id) {
                if (ev.schema_name == RecipeSourceRequested::zen_name) {
                    lookup = ev.seq;
                } else if (ev.schema_name == OpenSourceRequested::zen_name) {
                    open = ev.seq;
                }
            }
            if (ev.target != pane_id) {
                return;
            }
            if (ev.schema_name == loom::DispatchRefused::zen_name && ev.sender == stranger_id) {
                heard.push_back("forged");
            } else if (ev.schema_name == RecipeSourceSaid::zen_name) {
                heard.push_back("looked up");
            } else if (ev.schema_name == SourceOpened::zen_name) {
                heard.push_back("opened");
            }
        });
    b.enqueue_letter(input::scan::kE, "e");
    int turns = 0;
    while (lookup == 0) {
        REQUIRE(++turns < 8);
        REQUIRE(b.r.bus.pump_pending() > 0);
    }
    // THE FORGERY AT THE LOOKUP STAGE, delivered while the office still holds the answer.
    REQUIRE(office->held.valid());
    b.forge_refusal(pane_id, lookup, kProjectRole, RecipeSourceRequested::zen_name);
    b.r.bus.drain_until_idle();
    REQUIRE(heard == std::vector<std::string>{"forged"});
    CHECK(office->held.valid());
    CHECK(b.text().find("could not be looked up") == std::string::npos);
    // THE ANSWER, released; the open is asked.
    (void)b.r.bus.send(office_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
    while (open == 0) {
        REQUIRE(++turns < 16);
        REQUIRE(b.r.bus.pump_pending() > 0);
    }
    // THE FORGERY AT THE OPEN STAGE, delivered while the open is outstanding.
    b.forge_refusal(pane_id, open, kOpeningRole, OpenSourceRequested::zen_name);
    (void)b.r.bus.pump_pending();
    (void)b.r.bus.pump_pending();
    CHECK(b.text().find("could not reach") == std::string::npos);
    b.r.bus.drain_until_idle();
    b.r.bus.remove_observer(tap);
    CHECK(heard == std::vector<std::string>{"forged", "looked up", "forged", "opened"});
    REQUIRE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    CHECK(b.text().find("could not reach") == std::string::npos);
    CHECK(b.text().find("could not be looked up") == std::string::npos);
}

TEST_CASE("the Editor's refusal of a source reaches a narrow Builder row reason first, so the "
          "row's cut takes the tail and not why") {
    // THE ROW IS CUT AT ITS WIDTH FROM THE END, with the cut marked, so a refusal that led
    // with a file showed a narrow row the file and no reason. Every refusal of an open leads
    // with why: here the Editor's own, for a different source asked while its buffer is unsaved.
    BuilderRig b("bld-narrow-refusal");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    const std::string snake = (b.root / "snake.cpp").generic_string();
    b.next_source.known = true;
    b.next_source.kind = "single_source";
    b.next_source.source = snake;
    put_source(b.root / "snake.cpp", "int main() {}\n");
    put_source(b.root / "other.cpp", "int other() { return 1; }\n");
    b.open(120, 48, /*with_editor=*/true);
    // THE FIRST `e` OPENS snake.cpp, AND AN EDIT LEAVES IT UNSAVED.
    b.letter(input::scan::kE, "e");
    REQUIRE(b.r.session().panes.keyboard == b.editor_kind());
    b.r.text("x");
    REQUIRE(b.editor_status().find("UNSAVED") != std::string::npos);
    // THE SECOND `e`, BACK IN THE BUILDER, ASKS FOR ANOTHER SOURCE, AND THE EDITOR REFUSES IT.
    b.focus();
    b.next_source.source = (b.root / "other.cpp").generic_string();
    b.letter(input::scan::kE, "e");
    const std::string sentence = "the Editor holds unsaved changes to " + snake +
                                 " -- save source or discard source edits in the Editor first; "
                                 "nothing was opened";
    const ExternalPane* seat = b.r.session().panes.external_pane(b.kind);
    REQUIRE(seat != nullptr);
    // THE ROW IS NARROWER THAN THE SENTENCE, or this would prove nothing about a cut.
    REQUIRE(seat->columns > 0);
    REQUIRE(static_cast<std::size_t>(seat->columns) < sentence.size());
    std::string row;
    for (const std::string& shown : b.shown()) {
        if (shown.rfind("the Editor holds unsaved changes", 0) == 0) {
            row = shown;
        }
    }
    REQUIRE_MESSAGE(!row.empty(), b.text());
    CHECK(row.size() == static_cast<std::size_t>(seat->columns));
    CHECK(row == sentence.substr(0, static_cast<std::size_t>(seat->columns) - 3) + "...");
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos); // nothing else was opened
    CHECK(b.editor_status().find("other.cpp") == std::string::npos);
}

// =============================================================================
// The mouse: the Builder's controls, its recipe list, and the subjects they name
// =============================================================================
// The risks: a control acting on the wrong subject -- the three whose subject is not the weaver's
// choice -- a mode a hand cannot leave, and a press aimed at rows the pane has replaced.

namespace {

/// A POINTER BUTTON AT A PLACE IN THE BUILDER'S OWN ROOM, as the terminal medium reports it.
void bp_button(PaneRig& r, std::int64_t kind, std::int64_t button, bool pressed, std::int64_t row,
               std::int64_t column) {
    const ui::Rect body = external_body_rect(r.session(), kind);
    r.publish(loom::to_value(input::PointerButton{
        button, pressed, body.x + column,
        body.y + kExternalHeaderRows + row + surface::kTuiCanvasTopRow, input::space::kCells,
        input::mod::kNone}));
}

/// THE PICTURE THE BUILDER'S CANVAS HOLDS NOW, by its room and number -- what a case aims at
/// when it wants a press to name a picture drawn before a later one.
struct BpAim {
    std::int64_t grant = 0, picture = 0;
};
BpAim bp_aim(BuilderRig& b);
/// A PRIMARY PRESS AIMED AT THAT PICTURE, on a row and column of the canvas's lattice, said as
/// Workshop says one, with the keys already the pane's.
void bp_press_aimed(BuilderRig& b, const BpAim& aim, std::int64_t row, std::int64_t column);

struct BpFaceAt {
    std::int64_t row = -1;
    std::int64_t column = -1;
};
BpFaceAt bp_face_at(const std::vector<std::string>& rows, const std::string& face) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::size_t at = rows[i].find(face);
        if (at != std::string::npos) {
            return BpFaceAt{static_cast<std::int64_t>(i), static_cast<std::int64_t>(at)};
        }
    }
    return BpFaceAt{};
}

std::string bp_picture(const std::vector<std::string>& rows) {
    std::string out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out += std::to_string(i) + "| " + rows[i] + '\n';
    }
    return out;
}

/// THE FIRST ROW WHOSE TEXT BEGINS WITH `head`, or -1.
std::int64_t bp_row(const std::vector<std::string>& rows, const std::string& head) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].rfind(head, 0) == 0) {
            return static_cast<std::int64_t>(i);
        }
    }
    return -1;
}

/// PRESS THE CONTROL WHOSE FACE READS `face`, and require that it was drawn at all.
void bp_press_face(BuilderRig& b, const std::string& face) {
    const BpFaceAt at = bp_face_at(b.shown(), face);
    REQUIRE_MESSAGE(at.row >= 0, "no control read `", face, "` in\n", b.text());
    press_pane(b.r, b.kind, at.row, at.column + 1);
}

/// A BUILD THAT ENDED, arriving as the tool's own later publication -- the three fields that
/// make the ready state true (the load-after-build case above).
void bp_settled(BuilderRig& b, const char* recipe, const char* artifact, std::int64_t outcome) {
    b.tool->next.recipe = recipe;
    b.tool->next.artifact = artifact;
    b.tool->next.outcome = outcome;
    b.tool->next.realization = bld::realization::kNotAsked;
    b.tool_says();
}

} // namespace

TEST_CASE("the Builder's controls perform the operations its keys perform") {
    // THE MOUSE REACHES WHAT THE KEYS REACH, because both spend the same `perform`.
    BuilderRig b("bld-controls");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    REQUIRE_MESSAGE(b.text().find("one -> a  (1/2)") != std::string::npos, b.text());

    SUBCASE("build asks the tool for the chosen recipe") {
        bp_press_face(b, "[build]");
        REQUIRE(b.tool->asked.size() == 1);
        CHECK(b.tool->asked[0] == "one");
        CHECK(b.tool->realize_asked[0] == false);
    }
    SUBCASE("the load-after-build control turns the standing intent on and off") {
        bp_press_face(b, "[turn load-after-build on]");
        CHECK(b.text().find("load after build: on") != std::string::npos);
        REQUIRE(bp_face_at(b.shown(), "[turn load-after-build off]").row >= 0);
        CHECK(b.tool->asked.empty()); // a toggle sends nothing
        bp_press_face(b, "[turn load-after-build off]");
        CHECK(b.text().find("load after build: off") != std::string::npos);
    }
    SUBCASE("nothing built is nothing to load, and the control says so before it refuses") {
        REQUIRE(bp_face_at(b.shown(), "(load what was built)").row >= 0);
        bp_press_face(b, "(load what was built)");
        CHECK(b.text().find("nothing built is waiting to be loaded") != std::string::npos);
        CHECK(b.tool->asked.empty());
    }
    SUBCASE("nothing this Builder realized is standing, so promote and revert refuse") {
        b.author_height(30, 200, 60); // a room the whole strip fits in
        REQUIRE_MESSAGE(bp_face_at(b.shown(), "(promote the loaded image)").row >= 0, b.text());
        bp_press_face(b, "(promote the loaded image)");
        CHECK(b.text().find("nothing this Builder realized is standing") != std::string::npos);
        bp_press_face(b, "(revert the loaded image)");
        CHECK(b.text().find("nothing this Builder realized is standing") != std::string::npos);
        CHECK(b.tool->promotes.empty());
        CHECK(b.tool->reverts.empty());
    }
    SUBCASE("promote and revert name the artifact that is standing, and offer it") {
        b.author_height(30, 200, 60);
        bp_press_face(b, "[build]");
        b.tool->next.recipe = "one";
        b.tool->next.artifact = "a";
        b.tool->next.outcome = bld::outcome::kSucceeded;
        b.tool->next.realization = bld::realization::kRealized;
        b.tool_says();
        REQUIRE_MESSAGE(bp_face_at(b.shown(), "[promote a]").row >= 0, b.text());
        bp_press_face(b, "[promote a]");
        REQUIRE(b.tool->promotes == std::vector<std::string>{"a"});
        bp_press_face(b, "[revert a]");
        REQUIRE(b.tool->reverts == std::vector<std::string>{"a"});
    }
    SUBCASE("the recipe row and the choose control both open the list") {
        const std::int64_t row = bp_row(b.shown(), "recipe   one -> a");
        REQUIRE_MESSAGE(row >= 0, b.text());
        press_pane(b.r, b.kind, row, 0);
        CHECK(b.text().find("choose a recipe -- 2 recipes") != std::string::npos);
        bp_press_face(b, "[close the list]");
        CHECK(b.text().find("one -> a  (1/2)") != std::string::npos);
        bp_press_face(b, "[choose a recipe...]");
        CHECK(b.text().find("choose a recipe -- 2 recipes") != std::string::npos);
    }
}

TEST_CASE("WL-HAND-06: the Builder names its controls by operation and a recipe's row by the recipe") {
    BuilderRig b("bld-names");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    auto parts = held_parts(b.r.session(), b.kind);
    REQUIRE(parts.count("recipe:one") == 1);
    std::size_t controls = 0;
    for (const auto& [name, part] : parts) {
        if (name.rfind("control:", 0) != 0) continue;
        ++controls;
        CAPTURE(name);
        const std::string face = held_part_text(b.r.session(), b.kind, part);
        REQUIRE_FALSE(face.empty());
        CHECK((face.front() == '[' || face.front() == '('));
    }
    CHECK(controls > 0);
    // THE LIST: each recipe's row by its recipe, the cursor's or not.
    bp_press_face(b, "[choose a recipe...]");
    parts = held_parts(b.r.session(), b.kind);
    REQUIRE(parts.count("recipe:one") == 1);
    REQUIRE(parts.count("recipe:two") == 1);
    CHECK(parts.at("recipe:two").row == bp_row(b.shown(), "  two -> b"));
    press_pane(b.r, b.kind, parts.at("recipe:two").row, 0);
    REQUIRE(bp_row(b.shown(), "> two -> b") >= 0);
    CHECK(held_parts(b.r.session(), b.kind).at("recipe:two").row == bp_row(b.shown(), "> two -> b"));
}

TEST_CASE("the recipe list chooses by hand, and looking is not choosing") {
    // THE INTELLIGIBLE VISIBLE ROUTE TO A CHOICE. A weaver sees the catalog on rows, moves inside
    // it, and takes one -- and the cursor of the list is NOT the choice until they do, so merely
    // looking at a recipe cannot arm the next build against it.
    BuilderRig b("bld-list");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "c"}});
    b.open();
    bp_press_face(b, "[choose a recipe...]");
    REQUIRE_MESSAGE(b.text().find("choose a recipe -- 3 recipes") != std::string::npos, b.text());
    INFO("the list showed\n", b.text());
    CHECK(bp_row(b.shown(), "> one -> a") >= 0);
    CHECK(bp_row(b.shown(), "  two -> b") >= 0);

    SUBCASE("Escape leaves the choice exactly as it was, whatever the list's cursor did") {
        press_pane(b.r, b.kind, bp_row(b.shown(), "  three -> c"), 0);
        REQUIRE(bp_row(b.shown(), "> three -> c") >= 0);
        bp_press_face(b, "[close the list]");
        CHECK(b.text().find("one -> a  (1/3)") != std::string::npos);
        // ...AND NO BUILD FOLLOWS A CHOICE NOBODY MADE.
        bp_press_face(b, "[build]");
        REQUIRE(b.tool->asked.size() == 1);
        CHECK(b.tool->asked[0] == "one");
    }
    SUBCASE("a second press on the row the list stands on takes it") {
        const std::int64_t two = bp_row(b.shown(), "  two -> b");
        REQUIRE(two >= 0);
        press_pane(b.r, b.kind, two, 0); // the first press names the row
        REQUIRE(bp_row(b.shown(), "> two -> b") >= 0);
        CHECK(b.text().find("choose a recipe") != std::string::npos); // still choosing
        press_pane(b.r, b.kind, bp_row(b.shown(), "> two -> b"), 0);
        CHECK(b.text().find("build recipe: two -> b") != std::string::npos);
        CHECK(b.text().find("two -> b  (2/3)") != std::string::npos);
        bp_press_face(b, "[build]");
        REQUIRE(b.tool->asked.size() == 1);
        CHECK(b.tool->asked[0] == "two");
    }
    SUBCASE("the press that brings the keys back points at the pane and takes nothing") {
        // THE FOCUS RULE, ONE MODE OVER. A weaver coming back to this pane presses into it, and
        // that press must not also spend the choice the cursor happens to be standing on --
        // which is exactly what a press on the marked row means once the keys ARE here.
        press_pane(b.r, b.kind, bp_row(b.shown(), "  two -> b"), 0);
        REQUIRE(bp_row(b.shown(), "> two -> b") >= 0);
        b.unfocus();
        press_pane(b.r, b.kind, bp_row(b.shown(), "> two -> b"), 0);
        CHECK(b.text().find("choose a recipe") != std::string::npos); // still choosing
        CHECK(b.text().find("build recipe:") == std::string::npos);
        press_pane(b.r, b.kind, bp_row(b.shown(), "> two -> b"), 0);
        CHECK(b.text().find("build recipe: two -> b") != std::string::npos);
    }
    SUBCASE("the choose control takes the row the list stands on") {
        press_pane(b.r, b.kind, bp_row(b.shown(), "  three -> c"), 0);
        bp_press_face(b, "[choose this recipe]");
        CHECK(b.text().find("three -> c  (3/3)") != std::string::npos);
    }
    SUBCASE("a catalog republished under the list moves its cursor with the recipe it named") {
        press_pane(b.r, b.kind, bp_row(b.shown(), "  three -> c"), 0);
        REQUIRE(bp_row(b.shown(), "> three -> c") >= 0);
        b.tool->catalog = catalog_of({{"zero", "z"}, {"three", "c"}, {"one", "a"}});
        b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
        CHECK_MESSAGE(bp_row(b.shown(), "> three -> c") >= 0, b.text());
        bp_press_face(b, "[choose this recipe]");
        CHECK_MESSAGE(b.text().find("three -> c  (2/3)") != std::string::npos, b.text());
    }
}

TEST_CASE("the control that loads what was built names the BUILT recipe, not the choice") {
    // THE SUBJECT THAT IS NOT THE WEAVER'S CHOICE: `builder.build-realize` resends the FINISHED
    // build's recipe with the second intention aboard, so `load built a` loads `a` whatever the
    // weaver has picked since, and reading that face arms no next build. ⚔ MUTATIONS:
    // `load_built` sending `known_.recipes[cursor_row()].recipe` builds `two`; without its
    // `ready_to_load` guard the arm-only branch is never reached and arming while an artifact
    // stands built stops being refused.
    BuilderRig b("bld-load-built");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    bp_press_face(b, "[build]");
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    REQUIRE_MESSAGE(bp_face_at(b.shown(), "[load built a]").row >= 0, b.text());

    // THE CHOICE MOVES TO THE OTHER RECIPE, and the control still names what is standing built.
    bp_press_face(b, "[choose a recipe...]");
    press_pane(b.r, b.kind, bp_row(b.shown(), "  two -> b"), 0);
    bp_press_face(b, "[choose this recipe]");
    REQUIRE_MESSAGE(b.text().find("two -> b  (2/2)") != std::string::npos, b.text());
    REQUIRE_MESSAGE(bp_face_at(b.shown(), "[load built a]").row >= 0, b.text());

    const std::size_t sent = b.tool->asked.size();
    bp_press_face(b, "[load built a]");
    REQUIRE(b.tool->asked.size() == sent + 1);
    CHECK(b.tool->asked.back() == "one"); // the recipe that produced what is standing
    CHECK(b.tool->realize_asked.back() == true);
    CHECK(b.text().find("loading the built `a` now") != std::string::npos);
}

TEST_CASE("while an artifact stands built, arming the next build is a different answer "
          "and says so") {
    BuilderRig b("bld-arm-refused");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open();
    bp_press_face(b, "[build]");
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    REQUIRE_MESSAGE(bp_face_at(b.shown(), "(turn load-after-build on)").row >= 0, b.text());
    bp_press_face(b, "(turn load-after-build on)");
    CHECK(b.text().find("is built and waiting") != std::string::npos);
    CHECK(b.text().find("load after build: on") == std::string::npos);
    // ...AND THE KEY KEEPS BOTH MEANINGS IT HAD: `Shift+B` in the ready state is the button.
    b.r.key(input::scan::kB, input::mod::kShift);
    REQUIRE(b.tool->asked.size() == 2);
    CHECK(b.tool->asked[1] == "one");
    CHECK(b.tool->realize_asked[1] == true);
}

TEST_CASE("a right press offers the Builder's own rows, and a menu from another mode "
          "acts on nothing") {
    // THE SECOND BUTTON IS THE PANE'S FIRST (WL-CTX-08, WL-CTX-09). Every operation the strip
    // carries is in the menu too, spelled with the subject it will act on -- and a menu is
    // about the MODE it was opened in, so an answer arriving after the mode changed does
    // nothing at all.
    BuilderRig b("bld-menu");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);

    SUBCASE("a row of the Builder's menu builds the chosen recipe") {
        const std::int64_t recipe = bp_row(b.shown(), "recipe   one -> a");
        REQUIRE_MESSAGE(recipe >= 0, b.text());
        bp_button(b.r, b.kind, 3, true, recipe, 0);
        REQUIRE(menu_shown(b.r.session()));
        const std::vector<std::string> offered =
            context_rows_on(b.r.last_canvas(), b.r.session());
        INFO("the menu offered\n", bp_picture(offered));
        CHECK(any_row(offered, "choose a recipe from the list..."));
        // EVERY ROW THAT CAN NAME ITS SUBJECT DOES -- the three that act on the weaver's CHOICE
        // too -- so a menu standing open across a catalog that moved is judged against what it
        // promised rather than against whatever the choice became.
        CHECK(any_row(offered, "build `one`"));
        CHECK(any_row(offered, "turn load-after-build on"));
        CHECK(any_row(offered, "add `one`'s artifact to the load plan..."));
        CHECK(any_row(offered, "edit `one`'s source"));
        CHECK(any_row(offered, "hide pane")); // the standard rows, beneath the Builder's own
        CHECK_FALSE(any_row(offered, "manage this pane"));
        CHECK_FALSE(any_row(offered, "load the built")); // nothing is standing built
        CHECK(any_row(offered, "load what was built")); // ...and the row says so rather than
                                                        // vanishing out of the only fallback
        b.r.key(input::scan::kDown); // `build `one``
        b.r.key(input::scan::kReturn);
        CHECK_FALSE(menu_shown(b.r.session()));
        REQUIRE(b.tool->asked.size() == 1);
        CHECK(b.tool->asked[0] == "one");
    }
    SUBCASE("a right press on a fact row that names nothing is handed back to the host") {
        const std::int64_t fact = bp_row(b.shown(), "last ");
        REQUIRE_MESSAGE(fact >= 0, b.text());
        bp_button(b.r, b.kind, 3, true, fact, 0);
        CHECK(b.r.session().context.open);
        CHECK_FALSE(menu_shown(b.r.session()));
        CHECK(b.r.session().context.pane ==
              PaneRef{pane::kBuilderPaneRole, pane::kBuilderPane});
    }
}

TEST_CASE("a press that names a picture the Builder has replaced is refused in words") {
    // THE FENCE, MEASURED DIRECTLY. The Builder's rows move whenever what it was told changes,
    // so a press queued against an older picture must be refused rather than resolved against
    // the rows that arrived. The picture number is the pane's own, so a case has to say one --
    // which the office Workshop holds can.
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    REQUIRE(r.load(pane::kBuilderPaneStem, WORKSHOP_SO_BUILDER_PANE, pane::kBuilderPaneRole)
                .valid());
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.canvas_grant(m, pane::kBuilderPaneRole, pane::kBuilderPane, 10, 70);
    });
    REQUIRE_FALSE(watch->content.empty());
    const std::int64_t now = watch->pictures.back();
    CHECK(now != 0); // the Builder numbers its pictures

    std::int64_t control = -1;
    std::int64_t column = -1;
    for (std::size_t i = 0; i < watch->content.back().rows.size(); ++i) {
        const std::size_t at = watch->content.back().rows[i].text.find("[menu]");
        if (at != std::string::npos) {
            control = static_cast<std::int64_t>(i);
            column = static_cast<std::int64_t>(at) + 1;
        }
    }
    REQUIRE(control >= 0);

    SUBCASE("a picture not drawn yet") {
        const std::size_t said = watch->content.size();
        r.drive_watcher(watch, [control, column, now](PaneWatcher& wv, loom::Mail& m) {
            PaneCanvasPointer p = wv.canvas_at(control, column, canvas_pointer::kPress);
            p.picture = now + 7;
            p.keys_went_here = true;
            wv.point(m, pane::kBuilderPaneRole, p);
        });
        REQUIRE(watch->content.size() > said);
        CHECK(watch->content.back().rows[0].text.find("the rows moved -- press again") !=
              std::string::npos);
    }
    SUBCASE("the picture of a room since granted afresh, the same size") {
        PaneCanvasPointer old = watch->canvas_at(control, column, canvas_pointer::kPress);
        old.keys_went_here = true;
        r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
            wv.canvas_grant(m, pane::kBuilderPaneRole, pane::kBuilderPane, 10, 70);
        });
        REQUIRE(watch->canvas_room.grant != old.grant);
        CHECK(watch->content.back().rows[0].text.find("the rows moved") == std::string::npos);
        const std::size_t said = watch->content.size();
        r.drive_watcher(watch, [old](PaneWatcher& wv, loom::Mail& m) {
            wv.point(m, pane::kBuilderPaneRole, old);
        });
        REQUIRE(watch->content.size() > said);
        CHECK(watch->content.back().rows[0].text.find("the rows moved -- press again") !=
              std::string::npos);
    }
}

// =============================================================================
// Four gaps a drive through the real weave found, and the paths beside them
// =============================================================================
//
// A named operation that acted on whatever arrived while the weaver read it, a menu shortcut that
// ate text, a menu row answered with silence, a mode whose menu lacked its own controls.

namespace {

/// THE ROWS OF THE MENU THE PANE HAS OPEN.
std::vector<std::string> bp_menu_rows(BuilderRig& b) {
    REQUIRE(menu_shown(b.r.session()));
    return context_rows_on(b.r.last_canvas(), b.r.session());
}

/// OPEN THE PANE'S OWN MENU BY ITS CONTROL, and return what it offered.
std::vector<std::string> bp_open_menu(BuilderRig& b) {
    bp_press_face(b, "[menu]");
    return bp_menu_rows(b);
}

/// WALK THE PRESENTER'S CURSOR TO THE ROW READING `row` AND TAKE IT, as a weaver with the keys
/// does. The presenter opens standing on its first row.
void bp_choose_row(BuilderRig& b, const std::string& row) {
    const std::int64_t at = presented_line_of(b.r.session(), row);
    REQUIRE_MESSAGE(at >= 0, "no menu row read `", row, "` in\n",
                    bp_picture(context_rows_on(b.r.last_canvas(), b.r.session())));
    for (std::int64_t i = 0; i < at; ++i) {
        b.r.key(input::scan::kDown);
    }
    b.r.key(input::scan::kReturn);
}

/// WHAT THE TOOL WAS LAST ASKED TO BUILD, for a message that reads.
BpAim bp_aim(BuilderRig& b) {
    const ExternalPane* held = b.r.session().panes.external_pane(b.kind);
    REQUIRE(held != nullptr);
    REQUIRE(shows_canvas(*held));
    return BpAim{held->canvas.grant, held->canvas.content.picture};
}

void bp_press_aimed(BuilderRig& b, const BpAim& aim, std::int64_t row, std::int64_t column) {
    const ExternalPane* held = b.r.session().panes.external_pane(b.kind);
    REQUIRE(held != nullptr);
    const CanvasRows lattice = held_canvas_rows(*held);
    PaneCanvasPointer press{pane::kBuilderPane, aim.grant, aim.picture, 1, canvas_pointer::kPress, 1,
                            lattice.column_x(column), lattice.row_y(row)};
    press.keys_went_here = true;
    const auto sent = b.r.bus.office_send_to_role_as(
        b.r.workshop_id, kWorkshopProvider, pane::kBuilderPaneRole,
        loom::Message(loom::to_value(press), b.r.workshop_id, b.r.workshop_id, 0));
    REQUIRE(sent.valid());
    b.r.bus.drain_until_idle();
}

std::string bp_last_asked(BuilderRig& b) {
    return b.tool->asked.empty() ? std::string("nothing") : b.tool->asked.back();
}

} // namespace

TEST_CASE("an open menu row naming an artifact loads THAT artifact or refuses") {
    // A MENU STANDS OPEN ACROSS THE WEAVER'S OTHER ACTS AND EVERY BUILD THAT SETTLES UNDER IT. With
    // the constant `builder` as its subject, `load the built `a` now` stayed a row of this mode
    // when a newer status for `b` arrived, and choosing it built `b` while the sentence said `a`.
    // Each row's promise is kept beside the ask and established again when the answer lands.
    // ⚔ MUTATION: `offer_menu` recording no advertised subject for `kMenuLoadBuilt` asks for `two`.
    BuilderRig b("bld-menu-artifact");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    bp_press_face(b, "[menu]");
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "load the built `a` now"));

    // A NEWER BUILD SETTLES WHILE THE MENU STANDS OPEN.
    bp_settled(b, "two", "b", bld::outcome::kSucceeded);
    REQUIRE(menu_shown(b.r.session()));
    const std::string old_menu = bp_picture(bp_menu_rows(b));
    INFO("old menu still shown\n", old_menu);
    bp_choose_row(b, "load the built `a` now");
    INFO("pane after choice\n", b.text());
    CHECK_MESSAGE((b.tool->asked.empty() || b.tool->asked.back() == "one"),
                  "A choice naming artifact a must refuse or retain recipe one; observed ",
                  bp_last_asked(b));
    // ...AND THE REFUSAL SAYS WHAT WENT, rather than leaving the weaver to compare two names.
    CHECK_MESSAGE(b.text().find("`a` is not what is here now") != std::string::npos, b.text());
    CHECK(b.tool->asked.empty());
}

TEST_CASE("a numbered control naming an artifact is refused once that artifact is not "
          "what is standing") {
    // THE SAME AT THE CONTROL SEAM. A meaning of the action id alone made `[load built a]` and
    // `[load built b]` -- equal-width faces -- one span and one picture, so a press against the old
    // picture passed the fence and spent the newer subject. The advertised subject is part of the
    // meaning, so the picture moves with the promise. ⚔ MUTATIONS: `say_controls` recording `{}`
    // asks for `two`; `perform_on` without its comparison stays green here, the fence catching
    // it -- the comparison's own cases are `bld-menu-shared-stem` and the shared-stem control.
    BuilderRig b("bld-control-artifact");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const BpFaceAt aimed = bp_face_at(b.shown(), "[load built a]");
    REQUIRE(aimed.row >= 0);
    const BpAim old_picture = bp_aim(b);
    bp_settled(b, "two", "b", bld::outcome::kSucceeded);
    // THE PROMISE MOVED, SO WHAT THE PICTURE MEANS MOVED: equal-width faces are not equal spans,
    // and a press aimed at the older picture is refused.
    const std::string before_press = b.text();
    INFO("before old press\n", before_press);
    bp_press_aimed(b, old_picture, aimed.row, aimed.column + 1);
    CHECK_MESSAGE((b.tool->asked.empty() || b.tool->asked.back() == "one"),
                  "Old load-a control instead requested ", bp_last_asked(b));
    CHECK(b.tool->asked.empty());
}

TEST_CASE("the face drawn where the older one was is the one a press spends") {
    // THE OTHER SIDE OF THE SAME REPAIR, so the refusals above cannot be a pane that refuses
    // everything. At the columns `[load built a]` occupied, the newer build draws
    // `[load built b]` -- and a press there loads `b`, honestly and without a word of refusal.
    BuilderRig b("bld-control-unnumbered");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const BpFaceAt aimed = bp_face_at(b.shown(), "[load built a]");
    REQUIRE(aimed.row >= 0);
    bp_settled(b, "two", "b", bld::outcome::kSucceeded);
    // A PRESS AIMED AT THE FACE'S OWN PLACE, and the face at that place still reads `load built
    // b` -- the same columns, the same width, a different promise.
    REQUIRE(bp_face_at(b.shown(), "[load built b]").row == aimed.row);
    bp_press_face(b, "[load built b]"); // the weaver's press on what is drawn there now
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked.back() == "two"); // ...which is honest and acts
}

TEST_CASE("a held load menu cannot switch recipes sharing an artifact stem") {
    // TWO RECIPES PRODUCING ONE STEM (WL-PROJ-14): `one` and `two` both build `a`, so the menu's
    // advertised subject reads "a" before and after the second build settles, and only the
    // operation behind it (`target_op_of`) tells the two builds apart. ⚔ MUTATION: `target_op_of`
    // returning 0 unconditionally (or `perform_on` skipping the operation check) lets the name
    // alone match, and the stale choice asks the tool for `two` instead of refusing.
    BuilderRig b("bld-menu-shared-stem");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.tool->next.op = 11;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    bp_press_face(b, "[menu]");
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "load the built `a` now"));
    REQUIRE(b.tool->asked.empty());

    // A NEWER BUILD, OF A DIFFERENT RECIPE, PRODUCES THE SAME ARTIFACT NAME WHILE THE MENU
    // STANDS OPEN. The row's text cannot move -- it is still `a` -- so only the operation the
    // pane holds now differs from the one the row was written for.
    b.tool->next.op = 12;
    bp_settled(b, "two", "a", bld::outcome::kSucceeded);
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "load the built `a` now"));
    bp_choose_row(b, "load the built `a` now");
    INFO("old menu named the build from recipe one/op11; current build is two/op12\n", b.text());
    CHECK_MESSAGE((b.tool->asked.empty() || b.tool->asked.back() == "one"),
                  "An old load choice must refuse or preserve its build's recipe; observed ",
                  bp_last_asked(b));
    CHECK(b.tool->asked.empty());
    CHECK_MESSAGE(b.text().find("`a` is not what is here now") != std::string::npos, b.text());
}

TEST_CASE("a numbered load control preserves the build behind a shared artifact stem") {
    // THE SAME AT THE CONTROL-MAP SEAM: `[load built a]` is drawn at the same place, width and
    // text before and after the second build settles, so the operation rides in the meaning
    // beside the subject (`BuilderMeaning::op`) and an unmoved face still bumps the picture.
    // ⚔ MUTATIONS: `BuilderMeaning::operator==` ignoring `op` keeps one picture number and (with no
    // `perform_on` operation check) asks for `two`; `controls[...].subject_op` recorded as 0 fails
    // the same way through `say_controls`.
    BuilderRig b("bld-control-shared-stem");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open();
    b.tool->next.op = 11;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const BpFaceAt aimed = bp_face_at(b.shown(), "[load built a]");
    REQUIRE(aimed.row >= 0);
    const BpAim old_picture = bp_aim(b);
    b.tool->next.op = 12;
    bp_settled(b, "two", "a", bld::outcome::kSucceeded);
    // THE FACE DID NOT MOVE -- same row, same column, same text -- and what the picture means still
    // did, because the operation behind the unmoved face changed.
    const BpFaceAt still_there = bp_face_at(b.shown(), "[load built a]");
    REQUIRE_MESSAGE(still_there.row == aimed.row, b.text());
    REQUIRE_MESSAGE(still_there.column == aimed.column, b.text());
    bp_press_aimed(b, old_picture, aimed.row, aimed.column + 1);
    CHECK_MESSAGE((b.tool->asked.empty() || b.tool->asked.back() == "one"),
                  "An old numbered load control must refuse or preserve recipe one; observed ",
                  bp_last_asked(b));
    CHECK(b.tool->asked.empty());
}

TEST_CASE("rebuilding the same recipe replaces an offered load, and settling again does not") {
    // THE SAME RECIPE, REBUILT: recipe and artifact stay `one` -> `a` and only the OPERATION
    // changes. A rebuild under an old offer must not spend the rebuild, but progress on one
    // build -- a repeated status for the SAME operation, `still going` settling into
    // `succeeded` -- must never read as a different one, or the offer would refuse itself.
    BuilderRig b("bld-rebuild-same-recipe");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.tool->next.op = 21;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    bp_press_face(b, "[menu]");
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "load the built `a` now"));

    // ORDINARY PROGRESS ON THE SAME OPERATION: a repeated `succeeded` status for op 21, the way
    // a republished catalog or an unrelated frontier ask can re-settle the same fact. The menu
    // is untouched by this case, so re-choosing its row is still exercising op 21 -- proof the
    // repair does not refuse a build that never actually changed underneath it.
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    REQUIRE(menu_shown(b.r.session()));
    const std::size_t asked_before_rebuild = b.tool->asked.size();
    bp_choose_row(b, "load the built `a` now");
    REQUIRE(b.tool->asked.size() == asked_before_rebuild + 1);
    CHECK(b.tool->asked.back() == "one");
    CHECK(b.tool->realize_asked.back() == true);

    // A GENUINE REBUILD, offered again and chosen from a FRESH menu -- the positive control that
    // an ordinary, un-stale choice still works after a same-name rebuild.
    b.tool->next.op = 22;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    bp_press_face(b, "[menu]");
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "load the built `a` now"));
    const std::size_t asked_before_stale = b.tool->asked.size();

    // ...AND THE STALE CASE: op 23 replaces op 22 while THIS menu still stands, offered under
    // the same recipe and the same artifact name throughout.
    b.tool->next.op = 23;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    REQUIRE(menu_shown(b.r.session()));
    bp_choose_row(b, "load the built `a` now");
    CHECK_MESSAGE(b.tool->asked.size() == asked_before_stale,
                  "a same-name rebuild that replaced the offered operation was spent anyway");
    CHECK_MESSAGE(b.text().find("`a` is not what is here now") != std::string::npos, b.text());
}

TEST_CASE("the promote control acts on the image it names once a newer build makes it "
          "the one standing") {
    // THE SAME BOUNDARY, ON THE OTHER TWO CONTROLS THAT NAME THEIR SUBJECT -- A POSITIVE
    // CONTROL, not a refusal: what is STANDING moves with every build that settles, so
    // `[promote a]` is a promise with the same lifetime as `[load built a]`, and a press on
    // whatever is honestly drawn there NOW must still act, not just refuse the stale case. The
    // actual refusal -- a stale press across a shared artifact stem -- is the next case below.
    BuilderRig b("bld-promote-subject");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    b.author_height(30, 200, 60); // a room the whole strip fits in
    bp_press_face(b, "[build]");
    b.tool->next.recipe = "one";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    const BpFaceAt aimed = bp_face_at(b.shown(), "[promote a]");
    REQUIRE_MESSAGE(aimed.row >= 0, b.text());
    // A NEWER BUILD PUTS A DIFFERENT IMAGE THERE; the face moves, and so does the picture.
    b.tool->next.recipe = "two";
    b.tool->next.artifact = "b";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    REQUIRE_MESSAGE(bp_face_at(b.shown(), "[promote b]").row >= 0, b.text());
    const std::size_t promoted = b.tool->promotes.size();
    // THE WEAVER'S OWN PRESS ON WHAT IS DRAWN THERE NOW is honest and acts on `b`.
    bp_press_face(b, "[promote b]");
    REQUIRE(b.tool->promotes.size() == promoted + 1);
    CHECK(b.tool->promotes.back() == "b");
}

TEST_CASE("the promote and revert controls refuse a stale press across a shared artifact stem") {
    // THE SAME BOUNDARY BY A STALE PICTURE: two different recipes realizing one stem draw
    // `[promote a]` and `[revert a]` in the same place and width before and after, so only the
    // operation behind the meaning (`subject_op`) tells the realizations apart.
    // `PromoteArtifact`/`RevertArtifact` carry only the artifact name, so this is the pane's own
    // fence against a queued press spending whatever stands under that name -- not a claim that
    // a realization owner could tell the two builds apart.
    BuilderRig b("bld-promote-revert-shared-stem");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open();
    b.author_height(30, 200, 60); // a room the whole strip fits in, `[promote a]` included
    b.tool->next.op = 41;
    b.tool->next.recipe = "one";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    const BpFaceAt promote_aimed = bp_face_at(b.shown(), "[promote a]");
    const BpFaceAt revert_aimed = bp_face_at(b.shown(), "[revert a]");
    REQUIRE_MESSAGE(promote_aimed.row >= 0, b.text());
    REQUIRE_MESSAGE(revert_aimed.row >= 0, b.text());
    const BpAim old_picture = bp_aim(b);

    // A DIFFERENT RECIPE REALIZES THE SAME ARTIFACT NAME: the faces read exactly as they did.
    b.tool->next.op = 42;
    b.tool->next.recipe = "two";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    const BpFaceAt promote_still = bp_face_at(b.shown(), "[promote a]");
    const BpFaceAt revert_still = bp_face_at(b.shown(), "[revert a]");
    REQUIRE_MESSAGE(promote_still.row == promote_aimed.row, b.text());
    REQUIRE_MESSAGE(promote_still.column == promote_aimed.column, b.text());
    REQUIRE_MESSAGE(revert_still.row == revert_aimed.row, b.text());
    REQUIRE_MESSAGE(revert_still.column == revert_aimed.column, b.text());

    const std::size_t promoted = b.tool->promotes.size();
    const auto press_at = [&](const BpFaceAt& at) {
        bp_press_aimed(b, old_picture, at.row, at.column + 1);
    };
    press_at(promote_aimed);
    CHECK_MESSAGE(b.tool->promotes.size() == promoted,
                  "a stale promote press across a shared stem promoted anyway");

    const std::size_t reverted = b.tool->reverts.size();
    press_at(revert_aimed);
    CHECK_MESSAGE(b.tool->reverts.size() == reverted,
                  "a stale revert press across a shared stem reverted anyway");
}

TEST_CASE("a held promote or revert menu row cannot switch images sharing an artifact stem") {
    // THE MENU ROUTE'S OWN DISCRIMINATING CASE: a menu answers by `Offered::op` alone (the
    // picture fence of WL-HAND-03 never applies to a menu choice), so `perform_on`'s comparison
    // is this route's ONLY fence. Two recipes realizing one stem write `promote `a`...` and
    // `revert `a`...` in identical words both times. ⚔ MUTATION: `perform_on` without its
    // operation comparison (or `target_op_of` returning 0 for `kActionPromote`/`kActionRevert`)
    // promotes or reverts `two`'s realization instead of refusing.
    BuilderRig b("bld-menu-promote-revert-shared-stem");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);

    b.tool->next.op = 41;
    b.tool->next.recipe = "one";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    bp_press_face(b, "[menu]");
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "promote `a` -- a restart loads it"));
    REQUIRE(b.tool->promotes.empty());

    // A DIFFERENT RECIPE REALIZES THE SAME ARTIFACT NAME WHILE THE MENU STANDS OPEN. The row's
    // text cannot move -- still `promote `a`...` -- so only the operation behind it differs
    // from the one the row was written for.
    b.tool->next.op = 42;
    b.tool->next.recipe = "two";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "promote `a` -- a restart loads it"));
    const std::size_t promoted = b.tool->promotes.size();
    bp_choose_row(b, "promote `a` -- a restart loads it");
    INFO("old menu named the realization from recipe one/op41; current is two/op42\n", b.text());
    CHECK_MESSAGE(b.tool->promotes.size() == promoted,
                  "a stale promote choice across a shared stem promoted anyway");
    CHECK_MESSAGE(b.text().find("`a` is not what is here now") != std::string::npos, b.text());

    // THE SAME BOUNDARY, ON REVERT -- a fresh hold-then-go-stale cycle, since choosing a row
    // above already answered and closed that menu.
    b.tool->next.op = 43;
    b.tool->next.recipe = "one";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    bp_press_face(b, "[menu]");
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "revert `a` -- the previous image runs"));
    b.tool->next.op = 44;
    b.tool->next.recipe = "two";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    REQUIRE(menu_shown(b.r.session()));
    REQUIRE(any_row(bp_menu_rows(b), "revert `a` -- the previous image runs"));
    const std::size_t reverted = b.tool->reverts.size();
    bp_choose_row(b, "revert `a` -- the previous image runs");
    INFO("old menu named the realization from recipe one/op43; current is two/op44\n", b.text());
    CHECK_MESSAGE(b.tool->reverts.size() == reverted,
                  "a stale revert choice across a shared stem reverted anyway");
    CHECK_MESSAGE(b.text().find("`a` is not what is here now") != std::string::npos, b.text());
}

TEST_CASE("`edit source` in the recipe list opens the row the list is standing on, and leaves "
          "the choice alone") {
    // THE LIST MENU'S `edit `one`'s source` REACHES `edit_source`, which reads the LIST's cursor
    // while the list is open and leaves the committed choice alone (WL-PROJ-14: looking is not
    // choosing). A dispatcher with no branch for it closed the menu with no Editor and no word.
    // ⚔ MUTATIONS: no `kActionEditSource` branch in `choose_in_list` opens no Editor;
    // `edit_source` reading `cursor_row()` opens `two.cpp`, the choice and not the named row.
    BuilderRig b("bld-list-edit-source");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    put_source(b.root / "one.cpp", "// one\n");
    put_source(b.root / "two.cpp", "// two\n");
    for (const char* recipe : {"one", "two"}) {
        HostContext::RecipeSource said;
        said.known = true;
        said.kind = "single_source";
        said.source = (b.root / (std::string(recipe) + ".cpp")).generic_string();
        b.sources[recipe] = said;
    }
    b.open(160, 48, /*with_editor=*/true, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);

    // THE COMMITTED CHOICE IS `two`; THE LIST'S CURSOR IS PUT ON `one`. The two differ on
    // purpose, so this case proves WHICH source was opened and not merely that one was.
    bp_press_face(b, "[choose a recipe...]");
    press_pane(b.r, b.kind, bp_row(b.shown(), "  two -> b"), 0);
    bp_press_face(b, "[choose this recipe]");
    REQUIRE_MESSAGE(b.text().find("two -> b  (2/2)") != std::string::npos, b.text());
    bp_press_face(b, "[choose a recipe...]");
    press_pane(b.r, b.kind, bp_row(b.shown(), "  one -> a"), 0);
    REQUIRE_MESSAGE(bp_row(b.shown(), "> one -> a") >= 0, b.text());

    const std::vector<std::string> offered = bp_open_menu(b);
    INFO("the list menu offered\n", bp_picture(offered));
    REQUIRE(any_row(offered, "edit `one`'s source"));
    bp_choose_row(b, "edit `one`'s source");
    INFO("after edit-source choice\n", b.text());
    REQUIRE(b.r.session().panes.has(b.editor_kind()));
    CHECK(b.editor_status().find("one.cpp") != std::string::npos);
    CHECK(b.editor_status().find("two.cpp") == std::string::npos);
    // ...AND THE COMMITTED CHOICE IS UNTOUCHED: opening a source chooses nothing.
    bp_press_face(b, "[close the list]");
    CHECK_MESSAGE(b.text().find("two -> b  (2/2)") != std::string::npos, b.text());
}

TEST_CASE("a capital letter typed into the Builder's role line is text, not this pane's menu") {
    // THE ROLE LINE HAS NO DEFAULT MENU KEY. Workshop resolves the KEY TRANSITION before the
    // character arrives, so a menu on `Shift+M` opened on the `M` of a role like `Main` and the
    // letter was lost. ⚔ MUTATION: the role row declared on `kM`/`kShift` opens the menu and the
    // line reads `x`.
    BuilderRig b("bld-shift-m-text");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.r.key(input::scan::kO);
    REQUIRE_MESSAGE(b.text().find("type the role") != std::string::npos, b.text());
    b.r.text("x");
    REQUIRE_MESSAGE(b.text().find("role for a> x") != std::string::npos, b.text());
    b.r.key(input::scan::kM, input::mod::kShift);
    b.r.text("M");
    INFO("pane\n", b.text());
    CHECK_FALSE(menu_shown(b.r.session()));
    CHECK(b.text().find("role for a> xM") != std::string::npos);
    // AND THE MENU IS STILL ONE PRESS AWAY, which is what makes the missing key affordable.
    CHECK(any_row(bp_open_menu(b), "load `a` with the role typed"));
}

TEST_CASE("the role line keeps typed text visible in a narrow room") {
    // FILES' NARROW AUTHORING FIELD, ON THE BUILDER'S OWN LINE (`active_role_prompt`/
    // `active_prompt`): `role_prompt` grows with the stem being loaded, so a long artifact name
    // in a thirty-column room would leave the weaver's typing nowhere to show. Widening afterward
    // separates hidden text from lost text, as Files' own case does.
    BuilderRig b("bld-role-narrow");
    b.tool->catalog = catalog_of({{"one", "zengine-really-long-example"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.r.key(input::scan::kO);
    REQUIRE_MESSAGE(b.text().find("type the role") != std::string::npos, b.text());

    const Written narrow = author_pane_size(b.r.session().setup.active, builder_ref(),
                                            PaneSize{pane_unit::kPixels, cells_px(32)},
                                            PaneSize{pane_unit::kPixels, cells_px(9)});
    REQUIRE_MESSAGE(narrow.accepted, narrow.refusal);
    b.r.extent(160, 47);
    b.r.text("narrowvalue");
    INFO("narrow role line\n", b.text());
    CHECK_MESSAGE(b.text().find("narrowvalue") != std::string::npos,
                  "the role line accepted text but its full prompt hid that text");

    // SEPARATE ACCEPTANCE FROM VISIBILITY: the same draft reveals its text once wider.
    const Written wide = author_pane_size(b.r.session().setup.active, builder_ref(),
                                          PaneSize{pane_unit::kPixels, cells_px(80)},
                                          PaneSize{pane_unit::kPixels, cells_px(9)});
    REQUIRE_MESSAGE(wide.accepted, wide.refusal);
    b.r.extent(160, 48);
    CHECK_MESSAGE(b.text().find("role for zengine-really-long-example> narrowvalue") !=
                      std::string::npos,
                  b.text());
}

TEST_CASE("a Builder menu choice that opens the role line takes the keyboard across the door "
          "it waits on") {
    // A CHOSEN ROW THAT BEGINS AN EDIT TAKES THE KEYS, as in Files, one delivery later: the role
    // line opens only once the plan office has answered, so the choice's number rides across that
    // round trip and the grab is spent there. ⚔ MUTATIONS: `names_.choice` never set leaves the
    // typed role reaching nothing; `take_keys` in `chose` spends the grab before the line exists.
    BuilderRig b("bld-menu-takes-keys");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.unfocus();
    REQUIRE(b.r.session().panes.keyboard != b.kind);
    const std::int64_t recipe = bp_row(b.shown(), "recipe   one -> a");
    REQUIRE_MESSAGE(recipe >= 0, b.text());
    bp_button(b.r, b.kind, 3, true, recipe, 0);
    REQUIRE(menu_shown(b.r.session()));
    CHECK(b.r.session().panes.keyboard != b.kind); // pointing is not typing
    bp_choose_row(b, "add `one`'s artifact to the load plan...");
    REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    CHECK(b.r.session().panes.keyboard == b.kind);
    b.r.text("example.tally");
    CHECK_MESSAGE(b.text().find("role for a> example.tally") != std::string::npos, b.text());
}

TEST_CASE("a Builder menu choice that opens no line leaves the keyboard where the weaver put it") {
    // THE OTHER HALF OF THE SAME RULE, and the one that keeps a right press focus-neutral: a
    // chosen row that merely operates takes no keys.
    BuilderRig b("bld-menu-keeps-keys");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.unfocus();
    const std::int64_t recipe = bp_row(b.shown(), "recipe   one -> a");
    REQUIRE_MESSAGE(recipe >= 0, b.text());
    bp_button(b.r, b.kind, 3, true, recipe, 0);
    REQUIRE(menu_shown(b.r.session()));
    bp_choose_row(b, "turn load-after-build on");
    CHECK(b.text().find("load after build: on") != std::string::npos); // the operation ran
    CHECK(b.r.session().panes.keyboard != b.kind);                    // ...the keys did not move
}

TEST_CASE("every control each Builder mode draws has a row in that mode's own menu") {
    // THE STRIP'S PROMISE, KEPT. A narrow strip drops what will not fit and writes
    // `+N in menu`; that sentence is true only if the menu carries the mode's whole list. A
    // control the mode draws UNAVAILABLE gets its row too, so a weaver who cannot reach an
    // operation is owed its refusal rather than silence.
    BuilderRig b("bld-menu-complete");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);

    SUBCASE("the Builder's own rows, with nothing standing built") {
        const std::vector<std::string> offered = bp_open_menu(b);
        INFO("offered\n", bp_picture(offered));
        for (const char* row : {"choose a recipe from the list...", "build `one`",
                                "turn load-after-build on", "load what was built",
                                "add `one`'s artifact to the load plan...",
                                "build what the project is waiting on",
                                "promote the loaded image", "revert the loaded image",
                                "edit `one`'s source", "read what a build said",
                                "hide pane"}) {
            CHECK_MESSAGE(any_row(offered, row), "the Builder menu has no row `", row, "`");
        }
    }
    SUBCASE("the recipe list") {
        bp_press_face(b, "[choose a recipe...]");
        const std::vector<std::string> offered = bp_open_menu(b);
        INFO("offered\n", bp_picture(offered));
        for (const char* row : {"choose `one`", "edit `one`'s source",
                                "leave the choice as it was", "hide pane"}) {
            CHECK_MESSAGE(any_row(offered, row), "the list menu has no row `", row, "`");
        }
    }
    SUBCASE("the role line") {
        b.r.key(input::scan::kO);
        REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
        const std::vector<std::string> offered = bp_open_menu(b);
        INFO("offered\n", bp_picture(offered));
        for (const char* row : {"load `a` with the role typed", "write nothing and load nothing",
                                "hide pane"}) {
            CHECK_MESSAGE(any_row(offered, row), "the role menu has no row `", row, "`");
        }
    }
}

TEST_CASE("the list's own double-click still takes the row it was aimed at") {
    // ⚠ WHY THE LIST'S TWO FACES NAME NO RECIPE: a face reading `choose `one`` would move this
    // strip's spans every time the weaver looked at another row, and the picture fence would then
    // refuse the second press of an ordinary double-click.
    BuilderRig b("bld-list-double-click");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "c"}});
    b.open();
    bp_press_face(b, "[choose a recipe...]");
    // THE LIST'S OWN OPENING SENTENCE IS SPENT FIRST, because spending a notice moves every row
    // under it -- which is a different fact from the one this case is about.
    press_pane(b.r, b.kind, bp_row(b.shown(), "  two -> b"), 0);
    const std::int64_t row = bp_row(b.shown(), "  three -> c");
    REQUIRE_MESSAGE(row >= 0, b.text());
    const BpAim before = bp_aim(b);
    press_pane(b.r, b.kind, row, 0); // the first names the row
    REQUIRE_MESSAGE(bp_row(b.shown(), "> three -> c") == row, b.text());
    // THE PICTURE'S MEANING DID NOT MOVE UNDER THE HAND. The list's faces name no recipe, so
    // moving the cursor changes no span, so the second press of the double-click, aimed at the
    // picture before the cursor moved, is still about what it was aimed at...
    bp_press_aimed(b, before, row, 0); // ...and takes it
    CHECK_MESSAGE(b.text().find("build recipe: three -> c") != std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("three -> c  (3/3)") != std::string::npos, b.text());
}

TEST_CASE("a reader waiting on its first page still offers its whole list in a narrow room") {
    // A READER WHOSE PAGE THE BUILDER HAS NOT ANSWERED draws a header and nothing else, and a
    // narrow strip leaves `[menu]` the only route, so the menu carries the reader's operations,
    // not `close` and `manage` alone. The reader over REAL output is the output suite's; this is
    // the state a fixture tool can hold open. ⚔ MUTATION: the reader's branch of `offer_menu`
    // reduced to `kMenuClose` fails both checks.
    BuilderRig b("bld-reader-waiting");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.tool->next.op = 1;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const Written sized = author_pane_size(b.r.session().setup.active, builder_ref(),
                                           PaneSize{pane_unit::kPixels, cells_px(30)},
                                           PaneSize{pane_unit::kPixels, cells_px(9)});
    REQUIRE_MESSAGE(sized.accepted, sized.refusal);
    b.r.extent(160, 47);
    b.r.key(input::scan::kL);
    REQUIRE_MESSAGE(b.text().find("output #1") != std::string::npos, b.text());
    REQUIRE_MESSAGE(b.text().find("asking the B") != std::string::npos, b.text()); // cut at 30
    REQUIRE(bp_face_at(b.shown(), "[older build]").row < 0);
    const std::vector<std::string> offered = bp_open_menu(b);
    INFO("output pane\n", b.text(), "menu\n", bp_picture(offered));
    CHECK(any_row(offered, "older"));
    CHECK(any_row(offered, "pan right"));
    CHECK(any_row(offered, "close this build's output"));
    // ...AND THE WAY OUT WORKS FROM HERE, which is the whole point of the fallback.
    bp_choose_row(b, "close this build's output");
    CHECK_MESSAGE(b.text().find("output #1") == std::string::npos, b.text());
}

TEST_CASE("the wheel walks the Builder's recipe list, three rows a notch, and means nothing on its facts") {
    BuilderRig b("bld-wheel");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "c"}, {"four", "d"}});
    b.open();
    const ui::Rect body = external_body_rect(b.r.session(), b.kind);
    // ON THE FACTS a notch walks nothing: there is no list there.
    const std::string facts = b.text();
    b.r.wheel_cell(-1.0, body.x + 2, body.y + 2);
    CHECK(b.text() == facts);
    // IN THE LIST a notch toward the weaver walks its cursor three rows down, and one away walks
    // it back.
    bp_press_face(b, "[choose a recipe...]");
    REQUIRE_MESSAGE(bp_row(b.shown(), "> one -> a") >= 0, b.text());
    b.r.wheel_cell(-1.0, body.x + 2, body.y + 2);
    CHECK_MESSAGE(bp_row(b.shown(), "> four -> d") >= 0, b.text());
    b.r.wheel_cell(1.0, body.x + 2, body.y + 2);
    CHECK_MESSAGE(bp_row(b.shown(), "> one -> a") >= 0, b.text());
}

TEST_CASE("the Builder's role line shows the medium's caret where the line's is, its column counted in what the row draws") {
    BuilderRig b("bld-role-caret");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    const ExternalPane* pane = b.r.session().panes.external_pane(b.kind);
    REQUIRE(pane != nullptr);
    CHECK(held_caret(*pane).row == surface::kNoCaret); // no line, no caret
    CHECK(wears_medium_ground(*pane)); // the rows lie on the medium's own ground
    b.r.key(input::scan::kO);
    REQUIRE_MESSAGE(b.text().find("type the role") != std::string::npos, b.text());
    b.r.text("main");
    const std::int64_t line = bp_row(b.shown(), "role for a> main");
    REQUIRE_MESSAGE(line >= 0, b.text());
    const std::int64_t prompt = static_cast<std::int64_t>(std::string("role for a> ").size());
    CHECK(held_caret(*pane).row == line);
    CHECK(held_caret(*pane).column == prompt + 4);
    // A CHARACTER THE ROW SPELLS IN ONE CELL counts one column, whatever its bytes: typed after
    // a two-byte character, the caret stands one cell past it.
    b.r.text("\xC3\xA9");
    CHECK(any_row(b.shown(), "role for a> main?"));
    CHECK(held_caret(*pane).column == prompt + 5);
    b.r.key(input::scan::kLeft);
    CHECK(held_caret(*pane).column == prompt + 4);
    // ...AND A PRESS IS READ IN THE SAME CHARACTERS: pressed on the cell past that character, the
    // caret stands there, not back where its bytes would put it.
    bp_press_aimed(b, bp_aim(b), line, prompt + 5);
    CHECK(held_caret(*pane).column == prompt + 5);
    bp_press_aimed(b, bp_aim(b), line, prompt + 4);
    CHECK(held_caret(*pane).column == prompt + 4);
}

// =============================================================================
// A guest's hand on the Builder: what its row holds, what its host keeps, and each hand's gestures
// =============================================================================
// The risks: a build, a plan row's write or the guests file reached by a key its row or host does
// not hold, and the weaver's own act refused for a moment that landed between gesture and send.

namespace {

/// WHAT WORKSHOP SAYS OF A GUEST WHOSE ROW HOLDS NO `build`, after the act the pane did not do.
const std::string kNoBuild = "guest 'agent' may not build here: its row has no `build`";

/// WALK THE PRESENTER'S CURSOR TO THE ROW READING `row` AND TAKE IT, with the guest's injected
/// keys: `bp_choose_row`, by the other hand.
void guest_choose_row(BuilderRig& b, guest_hand::GuestHand& g, const std::string& row) {
    const std::int64_t at = presented_line_of(b.r.session(), row);
    REQUIRE_MESSAGE(at >= 0, "no menu row read `", row, "` in\n",
                    bp_picture(context_rows_on(b.r.last_canvas(), b.r.session())));
    for (std::int64_t i = 0; i < at; ++i) {
        g.key(input::scan::kDown);
    }
    g.key(input::scan::kReturn);
}

/// TWO BEATS OF ONE BUILDER ACT AND A KEY BETWEEN THEM, numbered in the order the bus delivered
/// them. The turn ends where the pane hears Workshop's word on its gesture (`word`); the second
/// beat's ask is the one the pane sent in that delivery, its answer the reply under that ask's
/// number, and `typed_at` the first key Workshop hears after the word.
class BetweenBeats {
public:
    BetweenBeats(BuilderRig& rig, const char* ask, const char* answer)
        : b_(rig), pane_(rig.r.kernel.weave_id(pane::kBuilderPaneStem)), ask_shape_(ask),
          answer_shape_(answer),
          tap_(rig.r.bus.add_observer([this](const loom::BusEvent& ev) { see(ev); })) {}
    ~BetweenBeats() { b_.r.bus.remove_observer(tap_); }
    BetweenBeats(const BetweenBeats&) = delete;
    BetweenBeats& operator=(const BetweenBeats&) = delete;

    std::uint64_t word = 0;
    std::int64_t asked_at = 0;
    std::int64_t typed_at = 0;
    std::int64_t answered_at = 0;

private:
    void see(const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered) {
            return;
        }
        ++moment_;
        if (word == 0) {
            if (ev.target == pane_ && ev.schema_name == ws::PaneOperationAnswered::zen_name) {
                word = ev.seq;
                b_.r.bus.stop(); // the second beat's ask is queued, not delivered
            }
            return;
        }
        if (asked_at == 0 && ev.sender == pane_ && ev.dispatch_parent == word &&
            ev.schema_name == ask_shape_) {
            ask_ = ev.correlation;
            asked_at = moment_;
        } else if (typed_at == 0 && ev.target == b_.r.workshop_id &&
                   ev.schema_name == input::KeyPressed::zen_name) {
            typed_at = moment_;
        } else if (answered_at == 0 && asked_at != 0 && ev.target == pane_ &&
                   ev.correlation == ask_ && ev.schema_name == answer_shape_) {
            answered_at = moment_;
        }
    }

    BuilderRig& b_;
    loom::WeaveId pane_;
    std::string ask_shape_;
    std::string answer_shape_;
    std::uint64_t ask_ = 0;
    std::int64_t moment_ = 0;
    loom::ObserverId tap_;
};

} // namespace

TEST_CASE("a guest without build is refused b, B, f and load-built in words on either host, "
          "and the Builder asks for nothing") {
    // A GUEST'S `input` REACHES THE BUILDER'S KEYS AND NONE OF ITS BUILDS. Each act that builds or
    // loads asks Workshop class `build` before it sends, and a row without `build` is refused on
    // both hosts: the host says what a guest may write and type, the row what it may build. The
    // four routes are the four sends -- the build, the button, the frontier, and the menu's
    // load-built, which has no default key -- so one that skipped its ask reaches the tool here.
    bool development = false;
    SUBCASE("on a weaver's host") { development = false; }
    SUBCASE("on a development host") { development = true; }
    CAPTURE(development);
    BuilderRig b("bld-guest-no-build");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.frontier.waiting = true;
    b.frontier.artifact = "zengine-snake";
    b.open(240, 60, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    guest_hand::GuestHand g(b.r, {"input"}, development);

    // THE WEAVER'S BUILD WORKED, so `B` is the button and the menu offers what it made.
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->asked.size() == 1);
    bp_settled(b, "snake", "zengine-snake", bld::outcome::kSucceeded);
    REQUIRE_MESSAGE(b.text().find("loads zengine-snake now") != std::string::npos, b.text());
    const std::size_t asked = b.tool->asked.size();
    const std::int64_t builds = b.tool->builds;
    const auto refused = [&b, asked, builds](const std::string& undone) {
        CHECK(b.tool->asked.size() == asked);
        CHECK(b.tool->builds == builds);
        CHECK_MESSAGE(b.text().find(undone + " -- " + kNoBuild) != std::string::npos, b.text());
    };

    g.key(input::scan::kB);
    refused("`snake` was not built");
    g.key(input::scan::kB, input::mod::kShift);
    refused("`zengine-snake` was not loaded");
    CHECK(b.text().find("load after build: on") == std::string::npos); // the button, not the toggle
    g.key(input::scan::kF);
    refused("nothing was asked for");
    // LOAD-BUILT HAS NO DEFAULT KEY: the guest opens the pane's menu and takes its row.
    g.key(input::scan::kM, input::mod::kShift);
    REQUIRE(any_row(bp_menu_rows(b), "load the built `zengine-snake` now"));
    guest_choose_row(b, g, "load the built `zengine-snake` now");
    CHECK_FALSE(menu_shown(b.r.session()));
    refused("`zengine-snake` was not loaded");

    // ...AND THE WEAVER'S OWN `b`, ON THE SAME PANE AND HOST, BUILDS: the refusals are the guest's
    // row's, not a Builder that refuses everything.
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->asked.size() == asked + 1);
    CHECK(b.tool->asked.back() == "snake");
}

TEST_CASE("a guest with build builds the catalog in force, and on a weaver's host is refused "
          "the role line's commit before any row is written") {
    // `build` IS A ROW'S POWER, AND A PLAN ROW IS A FILE. A guest whose row holds `build` builds
    // the recipe the catalog in force names, as the weaver's `b` would. The role line's commit
    // writes the plan before it builds, so it asks `build` and `write` in one ask, and on a weaver's
    // host only the weaver's hand writes: the plan office hears nothing, and the role the weaver
    // typed stands on the line for the weaver to commit.
    BuilderRig b("bld-guest-build");
    b.tool->catalog =
        catalog_of({{"one", "a"}, {"snake", "zengine-snake"}}, "/project/build-recipes.json");
    b.open(240, 60);
    guest_hand::GuestHand g(b.r, {"input", "build"});
    const loom::WeaveId pane_id = b.r.kernel.weave_id(pane::kBuilderPaneStem);
    REQUIRE(pane_id.value != 0);
    int rows_asked = 0;
    const loom::ObserverId tap =
        b.r.bus.add_observer([&rows_asked, pane_id](const loom::BusEvent& ev) {
            if (ev.kind == loom::EventKind::Delivered && ev.sender == pane_id &&
                ev.schema_name == PlanRowRequested::zen_name) {
                ++rows_asked;
            }
        });

    // THE CATALOG IN FORCE, BUILT BY THE GUEST: the weaver chose `snake`, and the guest's `b` asks
    // the tool for that recipe by name.
    g.weaver_key(input::scan::kC);
    REQUIRE_MESSAGE(b.text().find("snake -> zengine-snake  (2/2)") != std::string::npos, b.text());
    g.key(input::scan::kB);
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    CHECK(b.tool->realize_asked[0] == false);
    CHECK_MESSAGE(b.text().find("asked the Builder for `snake`") != std::string::npos, b.text());

    // THE WEAVER OPENS THE LINE AND TYPES THE ROLE; THE GUEST'S RETURN COMMITS IT.
    g.weaver_key(input::scan::kO);
    REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    g.weaver_text("zengine.oven");
    REQUIRE_MESSAGE(b.text().find("role for zengine-snake> zengine.oven") != std::string::npos,
                    b.text());
    g.key(input::scan::kReturn);
    CHECK(rows_asked == 0);
    CHECK(b.authored.empty());
    CHECK(b.plan_rows.empty());
    CHECK(b.tool->asked.size() == 1);
    CHECK_MESSAGE(b.text().find("nothing was loaded and nothing was written -- this is a weaver's "
                                "host: only the weaver's hand writes a file here") !=
                      std::string::npos,
                  b.text());
    // ...AND THE LINE STILL HOLDS THE WEAVER'S TEXT, with its own rows declared.
    CHECK_MESSAGE(b.text().find("role for zengine-snake> zengine.oven") != std::string::npos,
                  b.text());
    CHECK(b.declared() == kRoleLineIds);

    // THE WEAVER'S OWN COMMIT OF THE SAME LINE WRITES THE ROW.
    g.weaver_key(input::scan::kReturn);
    b.r.bus.remove_observer(tap);
    CHECK(rows_asked == 1);
    REQUIRE(b.authored.size() == 1);
    CHECK(b.authored[0].stem == "zengine-snake");
    CHECK(b.authored[0].role == "zengine.oven");
    CHECK(b.authored[0].recipe == "snake");
}

TEST_CASE("the weaver's f, and the role line's commit then build, stand with a key typed "
          "between the two beats") {
    // ONE APPROVAL, ASKED AT THE GESTURE AND CARRIED TO THE SEND. Between an act's two beats the
    // weaver types on, and Workshop counts that key as the weaver's newest gesture, so a send that
    // asked again at its own beat would find its gesture old and refuse the build. Each turn ends
    // where the pane hears Workshop's word and the key is queued there: it lands after the second
    // beat's ask has gone out and before its answer arrives.
    SUBCASE("the frontier action, with a key between the frontier's ask and its answer") {
        BuilderRig b("bld-f-between");
        b.tool->catalog = catalog_of({{"one", "a"}, {"snake", "zengine-snake"}});
        b.frontier.waiting = true;
        b.frontier.artifact = "zengine-snake";
        b.open();
        REQUIRE(b.r.kernel.weave_id(pane::kBuilderPaneStem).value != 0);
        BetweenBeats beats(b, ProjectFrontierRequested::zen_name, ProjectFrontierSaid::zen_name);
        b.r.key(input::scan::kF);
        REQUIRE(beats.word != 0);
        REQUIRE(b.r.bus.pending() > 0);
        CHECK(b.tool->asked.empty()); // Workshop said yes; the frontier is asked, and unanswered
        b.enqueue_key(input::scan::kDown);
        b.settle();
        REQUIRE(beats.asked_at != 0);
        REQUIRE(beats.typed_at != 0);
        REQUIRE(beats.answered_at != 0);
        CHECK(beats.asked_at < beats.typed_at);
        CHECK(beats.typed_at < beats.answered_at);
        REQUIRE(b.tool->asked.size() == 1);
        CHECK(b.tool->asked[0] == "snake");
        CHECK(b.tool->realize_asked[0] == true);
    }
    SUBCASE("the role line's commit, with a key between the written row's send and its arrival") {
        BuilderRig b("bld-row-between");
        b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
        b.next_append.frontier = true;
        b.next_append.product = "/build/zengine-snake.so";
        b.next_append.detail = "waiting";
        b.open();
        b.letter(input::scan::kO, "o");
        b.r.text("zengine.oven");
        REQUIRE_MESSAGE(b.text().find("role for zengine-snake> zengine.oven") != std::string::npos,
                        b.text());
        REQUIRE(b.r.kernel.weave_id(pane::kBuilderPaneStem).value != 0);
        BetweenBeats beats(b, PlanRowRequested::zen_name, PlanRowWritten::zen_name);
        b.r.key(input::scan::kReturn);
        REQUIRE(beats.word != 0);
        CHECK(b.authored.empty()); // Workshop said yes; the row is asked, and not yet written
        b.enqueue_key(input::scan::kDown);
        b.settle();
        REQUIRE(beats.asked_at != 0);
        REQUIRE(beats.typed_at != 0);
        REQUIRE(beats.answered_at != 0);
        // THE PLAN OFFICE ANSWERS INSIDE THE DELIVERY THAT ASKED, so the key lands between the
        // answer's send and its arrival.
        CHECK(beats.asked_at < beats.typed_at);
        CHECK(beats.typed_at < beats.answered_at);
        REQUIRE(b.authored.size() == 1);
        CHECK(b.authored[0].role == "zengine.oven");
        REQUIRE(b.tool->asked.size() == 1);
        CHECK(b.tool->asked[0] == "snake");
        CHECK(b.tool->realize_asked[0] == true);
        CHECK_MESSAGE(b.text().find("loading `zengine-snake` now") != std::string::npos, b.text());
    }
}

TEST_CASE("the weaver's b stands when a guest's injected key lands before the Builder's approval") {
    // ONE COUNTER FOR BOTH HANDS MADE THIS STALE: an approval current only while its gesture is the
    // latest of every hand's lets a guest's key, landing between the weaver's `b` and the Builder's
    // ask, make the weaver's own act old and refuse the build. Each hand keeps its own latest, so
    // the guest's key is the guest's newest gesture and nobody else's.
    BuilderRig b("bld-guest-between");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    guest_hand::GuestHand g(b.r, {"input"});
    const loom::WeaveId pane_id = b.r.kernel.weave_id(pane::kBuilderPaneStem);
    REQUIRE(pane_id.value != 0);
    const loom::WeaveId workshop = b.r.workshop_id;
    // WHAT LANDED WHEN: the weaver's `b` at the pane, the guest's moment at Workshop, and the
    // pane's ask for Workshop's word on the build.
    std::int64_t moment = 0;
    std::int64_t acted_at = 0;
    std::int64_t guest_at = 0;
    std::int64_t asked_at = 0;
    const loom::ObserverId tap = b.r.bus.add_observer(
        [&moment, &acted_at, &guest_at, &asked_at, pane_id, workshop](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            ++moment;
            if (acted_at == 0 && ev.target == pane_id &&
                ev.schema_name == PaneActionRequested::zen_name) {
                acted_at = moment;
            } else if (guest_at == 0 && ev.target == workshop && ev.payload != nullptr &&
                       ev.schema_name == input::AttributedInput::zen_name &&
                       !loom::from_value<input::AttributedInput>(*ev.payload).local) {
                guest_at = moment;
            } else if (asked_at == 0 && ev.sender == pane_id && ev.target == workshop &&
                       ev.schema_name == ws::v2::PaneOperationRequested::zen_name &&
                       ev.schema_version == ws::v2::PaneOperationRequested::zen_version) {
                asked_at = moment;
            }
        });
    // `Home` IS A KEY THE BUILDER DECLARES NOTHING FOR WHILE BROWSING: the guest's moment means
    // nothing to the pane, and is a gesture of the guest's all the same.
    g.weaver_then_guest(
        {input::KeyPressed{input::scan::kB, "", input::mod::kNone},
         input::KeyReleased{input::scan::kB, "", input::mod::kNone}},
        {guest_hand::GuestHand::key_event(input::scan::kHome, input::mod::kNone, true),
         guest_hand::GuestHand::key_event(input::scan::kHome, input::mod::kNone, false)});
    b.r.bus.remove_observer(tap);
    REQUIRE(acted_at != 0);
    REQUIRE(guest_at != 0);
    REQUIRE(asked_at != 0);
    CHECK(acted_at < guest_at);
    CHECK(guest_at < asked_at);
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    CHECK(b.tool->realize_asked[0] == false);
    CHECK_MESSAGE(b.text().find("asked the Builder for `snake`") != std::string::npos, b.text());
}

TEST_CASE("a guest's choice in a menu the weaver opened is judged for the guest, whatever the guest pressed since") {
    // ONE NUMBER, TWO HANDS: the weaver's Shift+M opens the Builder's menu under its act's number,
    // and the guest's choice in it answers under that same number. The opening act is spent on the
    // menu, so only the guest's choice can approve what the pane asks next -- judged for the guest
    // and refused, since its row has no `build`, even when the guest's next key lands first.
    BuilderRig b("bld-guest-chooses");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open(240, 60, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    guest_hand::GuestHand g(b.r, {"input"}, /*development=*/true);
    g.weaver_key(input::scan::kM, input::mod::kShift);
    REQUIRE(menu_shown(b.r.session()));
    const std::int64_t at = presented_line_of(b.r.session(), "build `snake`");
    REQUIRE(at >= 0);
    for (std::int64_t i = 0; i < at; ++i) {
        g.key(input::scan::kDown);
    }
    // RETURN CHOOSES, AND THE GUEST'S NEXT KEY LANDS BEFORE THE PANE'S ASK: one batch.
    g.inject({guest_hand::GuestHand::key_event(input::scan::kReturn, input::mod::kNone, true),
              guest_hand::GuestHand::key_event(input::scan::kReturn, input::mod::kNone, false),
              guest_hand::GuestHand::key_event(input::scan::kHome, input::mod::kNone, true),
              guest_hand::GuestHand::key_event(input::scan::kHome, input::mod::kNone, false)});
    CHECK_FALSE(menu_shown(b.r.session()));
    CHECK(b.tool->asked.empty());
    CHECK_MESSAGE(b.text().find(std::string("`snake` was not built -- ") + kNoBuild) != std::string::npos,
                  b.text());
    // THE WEAVER'S OWN `b` BUILDS: the refusal was the guest's row's.
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
}

TEST_CASE("the weaver's b stands when the weaver's own next key lands before the Builder's approval") {
    // ONE POLL CARRIES BOTH KEYS, so Workshop routes the weaver's `l` before the Builder's ask for
    // `b` arrives: `l` is the weaver's latest gesture, and its action replaces the record of the
    // one `b` sent. The ask names no send, so it spends `b`'s act while unspent, and builds.
    BuilderRig b("bld-weaver-between");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    guest_hand::GuestHand g(b.r, {"input"});
    const loom::WeaveId pane_id = b.r.kernel.weave_id(pane::kBuilderPaneStem);
    REQUIRE(pane_id.value != 0);
    const loom::WeaveId workshop = b.r.workshop_id;
    std::int64_t moment = 0;
    std::int64_t actions = 0;
    std::int64_t second_at = 0;
    std::int64_t asked_at = 0;
    const loom::ObserverId tap = b.r.bus.add_observer(
        [&moment, &actions, &second_at, &asked_at, pane_id, workshop](const loom::BusEvent& ev) {
            if (ev.kind != loom::EventKind::Delivered) {
                return;
            }
            ++moment;
            if (ev.target == pane_id && ev.schema_name == PaneActionRequested::zen_name &&
                ++actions == 2) {
                second_at = moment;
            } else if (asked_at == 0 && ev.sender == pane_id && ev.target == workshop &&
                       ev.schema_name == ws::v2::PaneOperationRequested::zen_name &&
                       ev.schema_version == ws::v2::PaneOperationRequested::zen_version) {
                asked_at = moment;
            }
        });
    for (const std::int64_t scan : {input::scan::kB, input::scan::kL}) {
        g.weaver->push_back(input::KeyPressed{scan, "", input::mod::kNone});
        g.weaver->push_back(input::KeyReleased{scan, "", input::mod::kNone});
    }
    g.pump();
    b.r.bus.remove_observer(tap);
    REQUIRE(second_at != 0);
    REQUIRE(asked_at != 0);
    CHECK(second_at < asked_at);
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
}

TEST_CASE("on a development host a guest with build commits the role line, and its row is "
          "written and built") {
    // THE SAME COMMIT ON THE AGENT'S OWN HOST: there a guest's typed text rests in the line and its
    // hand writes, so the guest opens the line, types the role and commits it, the plan office
    // writes the row, and the row's built product finishes with the button's act -- the build the
    // commit's one ask approved, carried to the second beat.
    BuilderRig b("bld-guest-row-dev");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.next_append.frontier = true;
    b.next_append.product = "/build/zengine-snake.so";
    b.next_append.detail = "waiting";
    b.open(240, 60);
    guest_hand::GuestHand g(b.r, {"input", "build"}, /*development=*/true);

    g.key(input::scan::kO);
    REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    g.text("zengine.oven");
    REQUIRE_MESSAGE(b.text().find("role for zengine-snake> zengine.oven") != std::string::npos,
                    b.text());
    g.key(input::scan::kReturn);
    REQUIRE(b.authored.size() == 1);
    CHECK(b.authored[0].stem == "zengine-snake");
    CHECK(b.authored[0].role == "zengine.oven");
    CHECK(b.authored[0].recipe == "snake");
    CHECK(b.plan_rows == std::vector<std::string>{"zengine-snake"});
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    CHECK(b.tool->realize_asked[0] == true);
    CHECK_MESSAGE(b.text().find("loading `zengine-snake` now") != std::string::npos, b.text());
}

TEST_CASE("the Builder's edit-source asks class open for the source it found before it sends "
          "the open") {
    // WHAT OPENS IS KNOWN ONLY AT THE SECOND DOOR, so that is where `e` asks: the project office
    // names the file, the pane asks Workshop class `open` about exactly that path under the gesture
    // that began it, and only Workshop's yes sends `OpenSourceRequested`. On a weaver's host the
    // file a guest's hand may not open is the guests file, refused before the opening office hears.
    BuilderRig b("bld-edit-open-class");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    const std::filesystem::path source = std::filesystem::absolute(b.root / "snake.cpp");
    const std::filesystem::path guests = std::filesystem::absolute(b.root / "guests.json");
    put_source(source, "int main() {}\n");
    put_source(guests, "{\"version\": 2, \"host\": \"weaver\", \"guests\": []}\n");
    b.next_source.known = true;
    b.next_source.kind = "single_source";
    b.open(240, 60, /*with_editor=*/true);
    const loom::WeaveId pane_id = b.r.kernel.weave_id(pane::kBuilderPaneStem);
    REQUIRE(pane_id.value != 0);
    // WHAT THE PANE SAID, IN THE ORDER IT WAS DELIVERED: each ask for Workshop's word, each open.
    struct Said {
        std::string what;
        std::vector<std::string> classes;
        std::string subject;
    };
    std::vector<Said> said;
    const loom::ObserverId tap = b.r.bus.add_observer([&said, pane_id](const loom::BusEvent& ev) {
        if (ev.kind != loom::EventKind::Delivered || ev.sender != pane_id ||
            ev.payload == nullptr) {
            return;
        }
        if (ev.schema_name == ws::v2::PaneOperationRequested::zen_name &&
            ev.schema_version == ws::v2::PaneOperationRequested::zen_version) {
            const auto asked = loom::from_value<ws::v2::PaneOperationRequested>(*ev.payload);
            said.push_back(Said{"asked", asked.classes, asked.subject});
        } else if (ev.schema_name == OpenSourceRequested::zen_name) {
            said.push_back(
                Said{"open", {}, loom::from_value<OpenSourceRequested>(*ev.payload).path});
        }
    });
    const std::vector<std::string> open_class{ws::scope::kOpen};

    SUBCASE("the weaver's e asks about the source it found, then opens it") {
        b.next_source.source = source.generic_string();
        b.letter(input::scan::kE, "e");
        REQUIRE(said.size() == 2);
        CHECK(said[0].what == "asked");
        CHECK(said[0].classes == open_class);
        CHECK(said[0].subject == source.generic_string());
        CHECK(said[1].what == "open");
        CHECK(said[1].subject == source.generic_string());
        REQUIRE(b.r.session().panes.has(b.editor_kind()));
        CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    }
    SUBCASE("a guest's e toward the guests file is refused on a weaver's host, and nothing opens") {
        guest_hand::GuestHand g(b.r, {"input"});
        b.r.host.host_fact.guests_file = guests.generic_string();
        b.next_source.source = guests.generic_string();
        g.key(input::scan::kE);
        REQUIRE(said.size() == 1);
        CHECK(said[0].what == "asked");
        CHECK(said[0].classes == open_class);
        CHECK(said[0].subject == guests.generic_string());
        CHECK_FALSE(b.r.session().panes.has(b.editor_kind()));
        CHECK(b.r.session().panes.keyboard == b.kind);
        CHECK_MESSAGE(b.text().find("`snake`: the source was not opened -- this is a weaver's "
                                    "host: the guests file opens only by the weaver's hand") !=
                          std::string::npos,
                      b.text());
        // ...AND THE WEAVER'S OWN `e` OPENS THE SAME FILE: the guests file is the weaver's to edit.
        b.letter(input::scan::kE, "e");
        REQUIRE(said.size() == 3);
        CHECK(said[1].what == "asked");
        CHECK(said[2].what == "open");
        CHECK(said[2].subject == guests.generic_string());
        REQUIRE(b.r.session().panes.has(b.editor_kind()));
        CHECK(b.editor_status().find("guests.json") != std::string::npos);
    }
    b.r.bus.remove_observer(tap);
}

// ============================================================================
// The manual: the rows the Builder declares in each mode, and the page Workshop holds for it
// ============================================================================

namespace {

/// WALK THE BUILDER THROUGH EVERY MODE ITS ROWS CHANGE IN, calling `at` in each: browsing, the
/// recipe list, the output reader and the role line, each entered and left by its own keys.
void each_builder_mode(BuilderRig& b, const std::function<void(const char*)>& at) {
    const auto declares = [&b](const char* id) {
        const std::vector<std::string> now = b.declared();
        return std::find(now.begin(), now.end(), std::string(id)) != now.end();
    };
    REQUIRE(declares(pane::kActionBuild));
    at("browse");
    b.r.key(input::scan::kReturn);
    REQUIRE(declares(pane::kActionRecipeChoose));
    at("recipes");
    b.r.key(input::scan::kEscape);
    REQUIRE(declares(pane::kActionBuild));
    b.tool->next.recipe = "tally";
    b.tool->next.artifact = "tally";
    b.tool->next.op = 1;
    b.tool->next.outcome = bld::outcome::kFailed;
    b.tool_says();
    b.letter(input::scan::kL, "l");
    REQUIRE(declares(pane::kActionOutputClose));
    at("output");
    b.r.key(input::scan::kEscape);
    REQUIRE(declares(pane::kActionBuild));
    b.letter(input::scan::kO, "o");
    REQUIRE(declares(pane::kActionCommit));
    at("role");
    b.r.key(input::scan::kEscape);
    REQUIRE(declares(pane::kActionBuild));
}

/// THE BUILDER PANE'S CENSUS: its rows in each mode it is walked through, and what each mode says
/// of the others; every control it draws in a mode names one of that mode's rows (WL-HAND-07).
const CensusDriverOf kBuilderCensus{pane::kBuilderPaneRole, [] {
    OfficeCensus census;
    BuilderRig b("bld-census");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open();
    each_builder_mode(b, [&](const char* mode) {
        const RuntimePane* held = b.row();
        CHECK(held->mode == mode); // the mode the pane says is the one the census names
        const std::string unnamed = held_control_parts_problem(b.r.session(), b.kind);
        CHECK_MESSAGE(unnamed.empty(), "in `" << std::string(mode) << "`: " << unnamed);
        census_take(census, pane::kBuilderPaneRole, *held);
    });
    return census;
}};

} // namespace

TEST_CASE("the action census: the rows the Builder declares in each mode, with what each press "
          "causes and the mode it enters, and the desk's, are tests/actions.txt") {
    // EVERY OFFICE'S DRIVER, registered by the suite that drives its panes: once each.
    CHECK_MESSAGE(census_registered_twice().empty(),
                  "an office's census is registered twice: " << census_registered_twice().front());
    REQUIRE(census_drivers().count(pane::kBuilderPaneRole) == 1);
    std::map<std::string, OfficeCensus> driven;
    for (const auto& [office, drive] : census_drivers()) {
        driven[office] = drive();
    }
    // WRITTEN BESIDE THE BUILD AS IT STANDS, office by office, ready to diff.
    std::ofstream now(ZENGINE_ACTIONS_NOW, std::ios::binary);
    for (const auto& [office, census] : driven) {
        for (const std::string& line : census.lines) now << line << "\n";
    }
    now.close();
    // EACH OFFICE'S LINES ARE ITS PINNED ONES, line by line and in order...
    const std::vector<std::string> pinned = census_pinned();
    for (const auto& [office, census] : driven) {
        CAPTURE(office);
        const std::vector<std::string> own = census_pinned_of(office);
        for (const std::string& line : census.lines) {
            CHECK_MESSAGE(std::find(own.begin(), own.end(), line) != own.end(),
                          "declared and not in the census: `" << line << "` (the census as it "
                          "stands: " << ZENGINE_ACTIONS_NOW << ")");
        }
        for (const std::string& line : own) {
            CHECK_MESSAGE(std::find(census.lines.begin(), census.lines.end(), line) !=
                              census.lines.end(),
                          "in the census and declared by no shipped pane: `" << line << "`");
        }
        CHECK(census.lines == own);
        // ...AND WHAT EACH MODE SAID OF THE OTHERS IS WHAT THEIR ROWS HERE CAUSE.
        for (const auto& [pane_key, said] : census.elsewhere) {
            CAPTURE(pane_key);
            check_elsewhere(office, said, pinned);
        }
    }
    // ...AND NO PINNED ROW IS ABOUT AN OFFICE NO SUITE DRIVES, nor sits in another office's file.
    for (const std::string& line : pinned) {
        CHECK_MESSAGE(driven.count(census_office(line)) == 1,
                      "in the census, and no suite drives its office: `" << line << "`");
    }
    for (const std::filesystem::path& file : census_files()) {
        if (file.parent_path().filename() != "actions") continue;
        for (const std::string& line : census_lines_of(file)) {
            CHECK_MESSAGE(census_office(line) + ".txt" == file.filename().string(),
                          file.generic_string() << " holds a row of another office: `" << line
                                                << "`");
        }
    }
}

// ============================================================================
// The press witness: each declared action, pressed, sends nothing its row does not declare
// ============================================================================

namespace {

/// THE PANE PROTOCOL, by name in every version: what a pane says to Workshop about itself -- its
/// offer, rows, content, caret, menus, its answer to a manual's ask, and the approval it asks
/// before an act. A press's causes are every other send.
bool pane_protocol(const std::string& shape) {
    static const std::set<std::string> kSaid{
        PaneOffered::zen_name,           PaneActions::zen_name,
        PaneContent::zen_name,           PaneCanvasContent::zen_name,
        PaneCaret::zen_name,             PaneMenuRequested::zen_name,
        PanePassRequested::zen_name,     PaneKeyboardRequested::zen_name,
        PaneEscapeUnspent::zen_name,     PaneRevealRequested::zen_name,
        PaneManageRequested::zen_name,   PaneOperationRequested::zen_name,
        PaneDocumentShown::zen_name,     PaneSettingsDeclared::zen_name};
    return kSaid.count(shape) == 1;
}

/// EVERY SEND ONE PARTICIPANT MAKES OUTSIDE THE PANE PROTOCOL while this is held, as
/// `<shape>/<version>`: delivered or refused, to an office, a weave or anyone.
class SendsOf {
public:
    SendsOf(loom::Switchboard& bus, loom::WeaveId who) : bus_(bus) {
        tap_ = bus_.add_observer([this, who](const loom::BusEvent& ev) {
            if ((ev.kind == loom::EventKind::Delivered || ev.kind == loom::EventKind::Refused) &&
                ev.sender == who && !pane_protocol(ev.schema_name)) {
                sent.insert(ev.schema_name + "/" + std::to_string(ev.schema_version));
            }
        });
    }
    ~SendsOf() { bus_.remove_observer(tap_); }
    SendsOf(const SendsOf&) = delete;
    SendsOf& operator=(const SendsOf&) = delete;
    std::set<std::string> sent;

private:
    loom::Switchboard& bus_;
    loom::ObserverId tap_ = 0;
};

/// WHAT A ROW DECLARES, AS A SEND IS SEEN: each cause's shape and version.
std::set<std::string> declared_sends(const std::vector<ActionCause>& causes) {
    std::set<std::string> out;
    for (const ActionCause& c : causes) out.insert(c.shape + "/" + std::to_string(c.version));
    return out;
}

/// WHAT ONE PRESS SENT THAT ITS ROW DOES NOT DECLARE -- the witness's red -- and what it declares
/// that this press did not send, kept to be listed.
struct Pressed {
    std::set<std::string> undeclared;
};
Pressed judge_press(const std::set<std::string>& sent, const std::vector<ActionCause>& causes,
                    std::set<std::string>& reached) {
    Pressed out;
    const std::set<std::string> declared = declared_sends(causes);
    for (const std::string& s : sent) {
        if (declared.count(s) == 0) out.undeclared.insert(s);
        reached.insert(s);
    }
    return out;
}

/// LIST WHAT NO DRIVEN STATE SENT: a declared cause the suite never reached, said and written beside
/// the build, never failed -- "may" is what a row declares.
void list_unreached(const std::map<std::string, std::vector<ActionCause>>& declared,
                    const std::map<std::string, std::set<std::string>>& reached,
                    const std::string& title) {
    std::string path = ZENGINE_ACTIONS_NOW;
    const std::size_t at = path.find("actions-now");
    if (at != std::string::npos) path.replace(at, std::string("actions-now").size(), title);
    std::ofstream out(path, std::ios::binary);
    for (const auto& [key, causes] : declared) {
        const auto got = reached.find(key);
        for (const std::string& s : declared_sends(causes)) {
            if (got == reached.end() || got->second.count(s) == 0) {
                out << key << " " << s << "\n";
                MESSAGE("declared and not sent by any driven state: " << key << " " << s);
            }
        }
    }
}

/// THE STATE A BUILDER IS DRIVEN INTO BEFORE A PRESS: a build waiting to be loaded, or a
/// realization standing, with the project waiting on its artifact.
enum class Standing { kBuilt, kRealized };

/// ONE BUILDER IN `mode`, its room tall enough for every control, standing as `standing` says: the
/// rows it then declares are the ones a press is judged against.
void builder_in(BuilderRig& b, const std::string& mode, Standing standing) {
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.frontier = ProjectFrontier{true, "tally", 0};
    b.next_source.source = (b.root / "tally.cpp").generic_string();
    b.open();
    b.author_height(40, 160, 48);
    b.tool->next.recipe = "tally";
    b.tool->next.artifact = "tally";
    b.tool->next.op = 1;
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = standing == Standing::kRealized ? bld::realization::kRealized
                                                               : bld::realization::kNotAsked;
    b.tool_says();
    if (mode == pane::kModeRecipes) {
        b.r.key(input::scan::kReturn);
    } else if (mode == pane::kModeOutput) {
        b.letter(input::scan::kL, "l");
    } else if (mode == pane::kModeRole) {
        b.letter(input::scan::kO, "o");
        b.r.text("example.tally");
    }
    REQUIRE(b.row()->mode == mode);
}

/// PRESS ONE DECLARED ACTION AS A WEAVER DOES: its default key, else the control the pane draws
/// for it. False when it has neither -- an action the witness cannot drive.
bool press_action(BuilderRig& b, const v3::PaneActionRow& row) {
    if (is_bound(Gesture{row.scancode, row.modifiers})) {
        b.r.key(row.scancode, row.modifiers);
        return true;
    }
    const auto parts = held_parts(b.r.session(), b.kind);
    const auto control = parts.find("control:" + row.id);
    if (control == parts.end()) return false;
    press_pane(b.r, b.kind, control->second.row, control->second.column);
    return true;
}

} // namespace

TEST_CASE("the press witness: each action the Builder declares, pressed in each mode it is driven "
          "into, sends nothing beside the pane protocol that its row does not declare") {
    std::map<std::string, std::vector<ActionCause>> declared;
    std::map<std::string, std::set<std::string>> reached;
    struct Drive {
        const char* mode;
        Standing standing;
    };
    const Drive drives[] = {{pane::kModeBrowse, Standing::kBuilt},
                            {pane::kModeBrowse, Standing::kRealized},
                            {pane::kModeRecipes, Standing::kRealized},
                            {pane::kModeOutput, Standing::kRealized},
                            {pane::kModeRole, Standing::kRealized}};
    for (const Drive& drive : drives) {
        std::size_t rows = 0;
        {
            BuilderRig count("bld-press");
            builder_in(count, drive.mode, drive.standing);
            rows = count.row()->actions.size();
        }
        REQUIRE(rows > 0);
        // A PANE OF ITS OWN FOR EVERY PRESS, so no press is judged by what an earlier one left.
        for (std::size_t i = 0; i < rows; ++i) {
            BuilderRig b("bld-press");
            builder_in(b, drive.mode, drive.standing);
            const v3::PaneActionRow row = b.row()->actions[i];
            const std::string key = std::string(drive.mode) + " " + row.id;
            declared[key] = row.causes;
            CAPTURE(key);
            SendsOf sends(b.r.bus, b.r.bus.role_holder(pane::kBuilderPaneRole));
            REQUIRE_MESSAGE(press_action(b, row), "`" << row.id << "` has no key and no control");
            const Pressed judged = judge_press(sends.sent, row.causes, reached[key]);
            for (const std::string& s : judged.undeclared) {
                CHECK_MESSAGE(false, "`" << row.id << "` in `" << std::string(drive.mode) << "` sent " << s
                                         << ", which its row does not declare");
            }
        }
    }
    list_unreached(declared, reached, "causes-unsent-builder");
}

TEST_CASE("the press witness: each row the desktop declares, pressed, sends nothing beside the pane "
          "protocol that the row does not declare -- the application's, the Pane Manager's and the "
          "Manual's") {
    namespace dp = zengine::desktop_pane;
    std::map<std::string, std::vector<ActionCause>> declared;
    std::map<std::string, std::set<std::string>> reached;
    const auto rig = [](PaneRig& r) {
        r.mount_workshop();
        r.ready();
        r.extent(160, 48);
        REQUIRE(r.load(dp::kDesktopStem, WORKSHOP_SO_DESKTOP_PANE, kDesktopRole).valid());
    };
    const auto judge = [&](PaneRig& r, const std::string& key, std::int64_t sc,
                           std::int64_t mods, const std::vector<ActionCause>& causes) {
        declared[key] = causes;
        CAPTURE(key);
        SendsOf sends(r.bus, r.bus.role_holder(kDesktopRole));
        REQUIRE(is_bound(Gesture{sc, mods}));
        r.key(sc, mods);
        for (const std::string& s : judge_press(sends.sent, causes, reached[key]).undeclared) {
            CHECK_MESSAGE(false, "`" << key << "` sent " << s << ", which its row does not declare");
        }
    };
    std::size_t app_rows = 0;
    {
        PaneRig r;
        rig(r);
        app_rows = r.session().keymap.app.size();
    }
    REQUIRE(app_rows > 0);
    for (std::size_t i = 0; i < app_rows; ++i) {
        PaneRig r;
        rig(r);
        const AppRow row = r.session().keymap.app[i];
        judge(r, "desk " + row.id, row.gesture.scancode, row.gesture.modifiers, row.causes);
    }
    // ...EACH PANE'S ROWS WITH THAT PANE SHOWN AND HOLDING THE KEYS, brought there by its own row.
    struct Shown {
        const char* pane;
        std::int64_t scancode;
        std::int64_t modifiers;
    };
    for (const Shown& shown : {Shown{dp::kLauncherPane, input::scan::kP, input::mod::kCtrl},
                               Shown{dp::kManualPane, input::scan::kF, input::mod::kCtrl}}) {
        std::size_t rows = 0;
        {
            PaneRig r;
            rig(r);
            const RuntimePane* held = r.session().panes.runtime.find(kDesktopRole, shown.pane);
            REQUIRE(held != nullptr);
            rows = held->actions.size();
        }
        REQUIRE(rows > 0);
        for (std::size_t i = 0; i < rows; ++i) {
            PaneRig r;
            rig(r);
            r.key(shown.scancode, shown.modifiers);
            const RuntimePane* held = r.session().panes.runtime.find(kDesktopRole, shown.pane);
            REQUIRE(held != nullptr);
            REQUIRE(r.session().panes.keyboard == held->kind);
            const v3::PaneActionRow row = held->actions[i];
            judge(r, held->mode + " " + row.id, row.scancode, row.modifiers, row.causes);
        }
    }
    list_unreached(declared, reached, "causes-unsent-desktop");
}

// ============================================================================
// Availability on the wire: a part says what its face draws
// ============================================================================

namespace {

/// A READER OF ONE PANE'S PARTS, as any participant asks Workshop: the sixth version's reading.
struct ViewAskerState {
    std::int64_t heard = 0;
    ZEN_SHAPE(ViewAskerState, 1, ZEN_FIELD(heard));
};
class ViewAsker
    : public loom::WeaveBase<ViewAsker, ViewAskerState, loom::Accept<v6::PaneView, loom::Refused>,
                             loom::Emit<v6::PaneViewRequested>> {
public:
    void on(const v6::PaneView& view, loom::Mail&) { views.push_back(view); }
    void on(const loom::Refused& refused, loom::Mail&) { refusals.push_back(refused.reason); }
    std::vector<v6::PaneView> views;
    std::vector<std::string> refusals;
};

/// THE BUILDER'S READING AS WORKSHOP SAYS IT: its named parts, each with its availability.
v6::PaneView builder_view(BuilderRig& b) {
    auto held = std::make_unique<ViewAsker>();
    ViewAsker* asker = held.get();
    loom::Grant grant;
    grant.allow_to_role(v6::PaneViewRequested::zen_name, v6::PaneViewRequested::zen_version,
                        kWorkshopProvider);
    const loom::WeaveId id = b.r.bus.register_weave(std::move(held), std::move(grant));
    asker->zen_set_self(id);
    (void)b.r.bus.send_as_to_role(
        id, kWorkshopProvider,
        loom::Message(loom::to_value(v6::PaneViewRequested{pane::kBuilderPaneRole,
                                                           pane::kBuilderPane, 0, {}}),
                      id, loom::WeaveId{}, 0));
    b.r.bus.drain_until_idle();
    REQUIRE_MESSAGE(asker->refusals.empty(), asker->refusals.front());
    REQUIRE(asker->views.size() == 1);
    v6::PaneView out = asker->views.front();
    b.r.bus.unregister_weave(id);
    return out;
}

} // namespace

TEST_CASE("WL-CAUSE-03: a control the Builder draws in brackets reads available in its reading, "
          "and one drawn in parentheses reads unavailable -- the pane's word, not the drawing") {
    BuilderRig b("bld-available");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open();
    const v6::PaneView view = builder_view(b);
    REQUIRE_FALSE(view.in_flight);
    std::string names;
    for (const v2::PanePart& p : view.parts) names += p.name + " ";
    INFO("parts: ", names, " covered by: ", view.covered.by.size());
    const auto part = [&view](const std::string& name) -> const v2::PanePart* {
        for (const v2::PanePart& p : view.parts) {
            if (p.name == name) return &p;
        }
        return nullptr;
    };
    // A RECIPE IS CHOSEN AND NOTHING STANDS: `[build]` applies, `(promote the loaded image)` does
    // not, and each part says so beside its words.
    const v2::PanePart* build = part("control:builder.build");
    REQUIRE_MESSAGE(build != nullptr, b.text());
    CHECK(build->text == "[build]");
    CHECK(build->available);
    const v2::PanePart* promote = part("control:builder.promote");
    REQUIRE_MESSAGE(promote != nullptr, b.text());
    CHECK(promote->text.rfind("(promote", 0) == 0);
    CHECK_FALSE(promote->available);
    // ...AND EVERY CONTROL'S WORD AND FACE AGREE, in a picture holding both kinds.
    std::size_t controls = 0;
    for (const v2::PanePart& p : view.parts) {
        if (p.name.rfind("control:", 0) != 0 || p.text.empty()) continue;
        ++controls;
        CAPTURE(p.name);
        CHECK(p.available == (p.text.front() == '['));
    }
    CHECK(controls > 2);
    // ...AND A RECIPE'S ROW, WHICH PERFORMS NOTHING, IS AVAILABLE.
    const v2::PanePart* recipe = part("recipe:tally");
    if (recipe != nullptr) CHECK(recipe->available);
}

TEST_CASE("the manual the Builder answers is the page its build embedded, and names exactly the "
          "commands it declares in each mode") {
    namespace manual = zengine::manual;
    const manual::Manual& page = manual::pages::zengine_builder_pane::kManual;
    BuilderRig b("bld-manual-asked");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open();
    ManualReader reader(b.r, pane::kBuilderPaneRole);

    // THE WHOLE PAGE, ASKED WITH NO SECTION: the page this tree's build embedded, in order.
    const PaneDocumentShown whole = reader.ask(pane::kBuilderPane, "");
    CHECK(whole.refusal.empty());
    CHECK(whole.content_id == std::string(page.content_id));
    REQUIRE(whole.sections.size() == page.count);
    for (std::size_t i = 0; i < page.count; ++i) {
        CHECK(whole.sections[i].key == std::string(page.sections[i].key));
        CHECK(whole.sections[i].text == std::string(page.sections[i].text));
    }

    // EVERY ID THE PANE DECLARES, IN EVERY MODE, IS ANSWERED BY ITS OWN SECTION...
    std::set<std::string> declared;
    each_builder_mode(b, [&](const char* mode) {
        for (const v3::PaneActionRow& a : b.row()->actions) {
            declared.insert(a.id);
            const PaneDocumentShown one = reader.ask(pane::kBuilderPane, a.id);
            INFO("mode ", mode, ", id ", a.id);
            CHECK(one.refusal.empty());
            REQUIRE(one.sections.size() == 1);
            CHECK(one.sections[0].key == a.id);
            CHECK(manual::is_command_group(one.sections[0].group));
        }
    });
    // ...AND THE PAGE'S COMMANDS ARE EXACTLY THOSE: no section for an id the pane never declares.
    std::set<std::string> commands;
    for (const PaneDocumentSection& s : whole.sections) {
        if (manual::is_command_group(s.group) && s.key != "commands" &&
            s.key.rfind("commands-", 0) != 0) {
            commands.insert(s.key);
        }
    }
    CHECK(commands == declared);
    CHECK(declared.size() >= 29);
}

TEST_CASE("a section or a pane the Builder does not have is refused, naming what is missing") {
    BuilderRig b("bld-manual-refused");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open();
    ManualReader reader(b.r, pane::kBuilderPaneRole);

    const PaneDocumentShown section = reader.ask(pane::kBuilderPane, "builder.fly");
    CHECK(section.refusal == "the manual of zengine.builder-pane has no section `builder.fly`");
    CHECK(section.sections.empty());
    CHECK(section.section == "builder.fly");

    const PaneDocumentShown other = reader.ask("elsewhere", "");
    CHECK(other.refusal == "zengine.builder-pane offers no pane `elsewhere`");
    CHECK(other.sections.empty());
    CHECK(other.content_id.empty());
}

TEST_CASE("a Builder reloaded in place with other words answers only the new ones") {
    BuilderRig b("bld-manual-reload");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open();
    ManualReader reader(b.r, pane::kBuilderPaneRole);
    const PaneDocumentShown before = reader.ask(pane::kBuilderPane, "");
    REQUIRE(before.refusal.empty());
    REQUIRE(reader.ask(pane::kBuilderPane, pane::kActionBuild).refusal.empty());

    // THE SAME OFFICE, ITS SAME SOURCE, BUILT WITH ANOTHER PAGE, reloaded in place through the
    // real control door: the same weave id and its state, a new image, and its own words.
    b.r.enqueue_reload(pane::kBuilderPaneStem, WORKSHOP_SO_BUILDER_PANE_REWORDED);
    b.r.bus.drain_until_idle();
    REQUIRE(b.r.load_refusals.empty());
    const PaneDocumentShown after = reader.ask(pane::kBuilderPane, "");
    REQUIRE(after.refusal.empty());
    CHECK(after.content_id != before.content_id);
    CHECK(after.sections.size() < 10);
    const PaneDocumentShown about = reader.ask(pane::kBuilderPane, "about");
    REQUIRE(about.sections.size() == 1);
    CHECK(about.sections[0].text.find("reworded") != std::string::npos);
    // NOTHING OF THE OLD PAGE IS ANSWERED: its commands' sections went with its image.
    CHECK(reader.ask(pane::kBuilderPane, pane::kActionBuild).refusal ==
          "the manual of zengine.builder-pane has no section `builder.build`");
}

// ---- The `?`: a pane's manual shown by the desktop's Manual, with the keys in force -----------

namespace {

namespace dp = zengine::desktop_pane;

/// THE BUILDER'S HEADER ROW AS PAINTED, and the cells its `?` and gear stand in.
struct HeaderMarks {
    std::string row;
    std::int64_t manual_x = 0, gear_x = 0, y = 0;
};
HeaderMarks header_marks(BuilderRig& b, std::int64_t kind) {
    const ui::Rect rect = external_body_rect(b.r.session(), kind);
    HeaderMarks out;
    out.y = rect.y;
    out.manual_x = rect.x + rect.w - 1;
    out.gear_x = rect.x + rect.w - 3;
    const std::vector<std::string> rows = external_region_rows(b.r.last_canvas(), rect);
    out.row = rows.empty() ? std::string() : rows.front();
    return out;
}

std::int64_t manual_kind(BuilderRig& b) {
    const RuntimePane* row = b.r.session().panes.runtime.find(kDesktopRole, dp::kManualPane);
    return row == nullptr ? kNoPaneKind : row->kind;
}

bool on_desk(BuilderRig& b, std::int64_t kind) {
    for (const OpenPane& p : b.r.session().panes.open) {
        if (p.kind == kind) return true;
    }
    return false;
}

std::vector<std::string> manual_shown(BuilderRig& b) {
    const ExternalPane* pane = b.r.session().panes.external_pane(manual_kind(b));
    REQUIRE(pane != nullptr);
    return held_row_texts(*pane);
}

bool more_below(const std::vector<std::string>& rows) {
    return !rows.empty() && rows.back().find("more lines below") != std::string::npos;
}

/// PRESS INTO THE MANUAL, so its keys are the ones a key reaches.
void focus_manual(BuilderRig& b) {
    const std::int64_t kind = manual_kind(b);
    REQUIRE(kind != kNoPaneKind);
    if (b.r.session().panes.keyboard != kind) {
        const ui::Rect rect = external_body_rect(b.r.session(), kind);
        b.r.press_cell(rect.x, rect.y);
        REQUIRE(b.r.session().panes.keyboard == kind);
    }
}

/// THE MANUAL'S WHOLE TEXT, AS ITS PANE SHOWS IT: the pane pressed into, its window taken to the
/// top, then walked a line at a time with its own key, each new last line kept.
std::vector<std::string> read_manual(BuilderRig& b) {
    focus_manual(b);
    b.r.key(input::scan::kHome);
    std::vector<std::string> rows = manual_shown(b);
    REQUIRE(rows.size() >= 2);
    std::vector<std::string> lines(rows.begin() + 1, rows.end() - (more_below(rows) ? 1 : 0));
    for (int guard = 0; more_below(rows) && guard < 4000; ++guard) {
        b.r.key(input::scan::kDown);
        rows = manual_shown(b);
        REQUIRE(rows.size() >= 2);
        lines.push_back(rows[rows.size() - (more_below(rows) ? 2 : 1)]);
    }
    CHECK_FALSE(more_below(rows));
    return lines;
}

bool has_line(const std::vector<std::string>& lines, const std::string& line) {
    return std::find(lines.begin(), lines.end(), line) != lines.end();
}

/// The lines of one command's section in the shown manual: from its heading to the next heading,
/// a command's (`<id> -- <keys>`) or a group's (`Commands ...`).
std::string section_of(const std::vector<std::string>& lines, const std::string& id) {
    static const std::regex heading("^([a-z][a-z0-9-]*\\.[a-z0-9.-]+ -- |Commands)");
    std::string out;
    bool in = false;
    for (const std::string& line : lines) {
        if (line.rfind(id + " -- ", 0) == 0) {
            in = true;
            continue;
        }
        if (in && std::regex_search(line, heading)) break;
        if (in) out += line + "\n";
    }
    return out;
}

/// THE BUILDER'S COMMANDS AS THE CENSUS PINS THEM: each id with every default key it declares.
std::map<std::string, std::set<std::string>> builder_census() {
    std::map<std::string, std::set<std::string>> out;
    for (const std::string& line : census_pinned()) {
        std::istringstream in(line);
        std::string office, mode, id, key;
        in >> office >> mode >> id >> key;
        if (office == pane::kBuilderPaneRole) out[id].insert(key);
    }
    return out;
}

std::string joined_pairs(const std::vector<std::string>& pairs) {
    std::string out;
    for (const std::string& pair : pairs) out += pair + " | ";
    return out;
}

/// CHOOSE ONE ROW OF THE HOST'S OPEN MENU, by its action id, with the menu's own keys.
void choose_context(BuilderRig& b, const std::string& id) {
    const std::vector<ContextEntry> rows = context_population(b.r.session().context);
    std::size_t at = rows.size();
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (!rows[i].is_group && rows[i].row != nullptr && id == rows[i].row->id) at = i;
    }
    REQUIRE(at < rows.size());
    for (std::size_t i = b.r.session().context.cursor; i < at; ++i) b.r.key(input::scan::kDown);
    REQUIRE(b.r.session().context.cursor == at);
    b.r.key(input::scan::kReturn);
}

} // namespace

TEST_CASE("WL-CHROME-09: a pane whose holder answers its manual wears a `?` and a gear at its top "
          "right; one that does not, or a desk with no row to show one, wears neither") {
    // THE ROW THE HOST DRAWS THE `?` FOR is the one the desktop declares, spelled in both.
    CHECK(std::string(kManualAction) == dp::kActionManual);
    {
        BuilderRig b("bld-help-marks");
        b.tool->catalog = catalog_of({{"tally", "tally"}});
        b.open(160, 48, false, true, true, false, /*with_desktop=*/true);
        const HeaderMarks marks = header_marks(b, b.kind);
        CHECK(b.r.session().panes.external_pane(b.kind)->manual);
        REQUIRE(marks.row.size() >= 3);
        CHECK(marks.row.substr(marks.row.size() - 3) == kChromeMarks);
        // ...AND SO DOES THE DESK'S OWN PANE MANAGER, whose holder answers its manual too.
        b.unfocus();
        b.r.key(input::scan::kP, input::mod::kCtrl);
        const RuntimePane* manager =
            b.r.session().panes.runtime.find(kDesktopRole, dp::kLauncherPane);
        REQUIRE(manager != nullptr);
        REQUIRE(on_desk(b, manager->kind));
        CHECK(b.r.session().panes.external_pane(manager->kind)->manual);
        const HeaderMarks worn = header_marks(b, manager->kind);
        REQUIRE(worn.row.size() >= 3);
        CHECK(worn.row.substr(worn.row.size() - 3) == kChromeMarks);
        // A PANE WHOSE HOLDER ANSWERS NO MANUAL -- the fixture greeting -- wears neither mark.
        b.unfocus();
        REQUIRE(b.r.load("zengine-workshop-hello", WORKSHOP_SO_HELLO, kHelloOffice).valid());
        b.r.pick(PaneRef{kHelloOffice, kHelloPane});
        const RuntimePane* hello = b.r.session().panes.runtime.find(kHelloOffice, kHelloPane);
        REQUIRE(hello != nullptr);
        REQUIRE(on_desk(b, hello->kind));
        CHECK_FALSE(b.r.session().panes.external_pane(hello->kind)->manual);
        const HeaderMarks bare = header_marks(b, hello->kind);
        CHECK(bare.row.find(kChromeMarks) == std::string::npos);
        CHECK(chrome_mark_at(b.r.session().panes, b.r.session().setup.active,
                             screen_of(b.r.session()), hello->kind, b.r.session().pane_titles,
                             input::space::kCells, bare.manual_x, bare.y) == chrome_mark::kNone);
    }
    {
        // WITH NO DESKTOP THERE IS NO ROW THAT SHOWS A MANUAL, so the Builder wears no `?`.
        BuilderRig b("bld-help-no-desk");
        b.tool->catalog = catalog_of({{"tally", "tally"}});
        b.open();
        CHECK_FALSE(b.r.session().panes.external_pane(b.kind)->manual);
        CHECK(header_marks(b, b.kind).row.find(kChromeMarks) == std::string::npos);
    }
}

TEST_CASE("WL-DESK-17: the Builder's `?` shows About, Use and every command it declares in every "
          "mode, each beside the key it answers to, the census's default where nothing moved it") {
    BuilderRig b("bld-help-shown");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open(160, 48, false, true, true, false, /*with_desktop=*/true);
    const HeaderMarks marks = header_marks(b, b.kind);
    b.r.press_cell(marks.manual_x, marks.y);
    REQUIRE(manual_kind(b) != kNoPaneKind);
    REQUIRE(on_desk(b, manual_kind(b)));
    const std::vector<std::string> head = manual_shown(b);
    REQUIRE_FALSE(head.empty());
    CHECK(head[0] == "MANUAL -- zengine.builder-pane/builder");

    const std::vector<std::string> lines = read_manual(b);
    const auto about = std::find(lines.begin(), lines.end(), "About");
    const auto use = std::find(lines.begin(), lines.end(), "Use");
    const auto commands = std::find(lines.begin(), lines.end(), "Commands");
    REQUIRE(about != lines.end());
    REQUIRE(use != lines.end());
    REQUIRE(commands != lines.end());
    CHECK(about < use);
    CHECK(use < commands);
    // EVERY ID THE CENSUS HOLDS FOR THE BUILDER, in every mode, under its own heading with its key.
    const auto census = builder_census();
    REQUIRE(census.size() >= 29);
    for (const auto& [id, keys] : census) {
        INFO("command ", id);
        bool headed = false;
        for (const std::string& line : lines) {
            if (line.rfind(id + " -- ", 0) != 0) continue;
            headed = true;
            for (const std::string& key : keys) {
                if (key != "unbound") { // a mode where the command has no key adds none
                    CHECK(line.find(key) != std::string::npos);
                }
            }
        }
        CHECK(headed);
    }
    // THE KEY MARKERS SHOW KEYS, never the markers: no `<!--` reaches the weaver.
    for (const std::string& line : lines) {
        CHECK(line.find("<!--") == std::string::npos);
    }
    CHECK(section_of(lines, pane::kActionBuild).find("press `b`") != std::string::npos);
}

TEST_CASE("WL-DESK-17: a key the weaver moved in a keymap file shows in place of the marked "
          "default, beside the heading and in the words, for a command in force and one in "
          "another mode") {
    BuilderRig b("bld-help-moved");
    const std::string path = (b.root / "keymap.json").generic_string();
    write_keymap_file(path, keymap_file_text("default", {{pane::kActionBuild, "g"},
                                                         {"builder.output-older", "9"}}));
    b.r.host.keymap_path = path;
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open(160, 48, false, true, true, false, /*with_desktop=*/true);
    const HeaderMarks marks = header_marks(b, b.kind);
    b.r.press_cell(marks.manual_x, marks.y);
    const std::vector<std::string> lines = read_manual(b);
    CHECK(has_line(lines, "builder.build -- g"));
    const std::string build = section_of(lines, pane::kActionBuild);
    CHECK(build.find("press `g`") != std::string::npos);
    CHECK(build.find("`b`") == std::string::npos);
    // ...AND FOR A COMMAND OF THE OUTPUT READER, which is not in force while the pane browses.
    CHECK(has_line(lines, "builder.output-older -- 9"));
    CHECK(section_of(lines, "builder.output-older").find("`9`") != std::string::npos);
}

TEST_CASE("WL-KEY-16: the band's spelling and the Hotkeys view's are set apart from a pane's gear, "
          "each changing only its own surface, the `?` spelling as the Hotkeys view does") {
    BuilderRig b("bld-help-spelling");
    b.r.host.prefs_path = (b.root / "workshop-prefs.json").generic_string();
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open(160, 48, false, true, true, false, /*with_desktop=*/true);
    const auto band = [&] {
        return joined_pairs(help_pairs(b.r.session().keymap, b.r.session().band_keys,
                                       KeyContext::kPane, b.kind));
    };
    const auto hotkeys = [&] {
        for (const ShownBinding& row :
             keymap_shown(b.r.session(), std::string(), std::string()).rows) {
            if (row.id == pane::kActionBuildRealize) return row.shown;
        }
        return std::string();
    };
    const auto manual_heading = [&] {
        for (const std::string& line : manual_shown(b)) {
            if (line.rfind("builder.build-realize -- ", 0) == 0) return line;
        }
        return std::string();
    };
    // THE DEFAULTS, AS THEY WERE: the band short, the Hotkeys view and the `?` full.
    CHECK(band().find("B load after build") != std::string::npos);
    CHECK(hotkeys() == "shift+b");
    const HeaderMarks marks = header_marks(b, b.kind);
    b.r.press_cell(marks.manual_x, marks.y);
    focus_manual(b);
    for (int i = 0; i < 400 && manual_heading().empty() && more_below(manual_shown(b)); ++i) {
        b.r.key(input::scan::kDown);
    }
    CHECK(manual_heading() == "builder.build-realize -- shift+b");

    // THE GEAR OPENS THE HOST'S OWN MENU OF SETTINGS, ABOUT THAT PANE, and only them.
    b.focus();
    b.r.press_cell(marks.gear_x, marks.y);
    REQUIRE(b.r.session().context.open);
    CHECK(b.r.session().context.subject == context_subject::kSettings);
    CHECK(b.r.session().context.pane == PaneRef{pane::kBuilderPaneRole, pane::kBuilderPane});
    std::vector<std::string> settings;
    for (const ContextEntry& e : context_population(b.r.session().context)) {
        settings.push_back(e.row != nullptr ? e.row->id : e.group);
    }
    CHECK(settings == std::vector<std::string>{"workshop.pane-titles", "workshop.band-keys",
                                               "workshop.hotkeys-keys"});
    choose_context(b, "workshop.band-keys");
    CHECK(b.r.session().band_keys == KeySpelling::kFull);
    b.focus();
    CHECK(band().find("shift+b load after build") != std::string::npos);
    CHECK(hotkeys() == "shift+b");                                  // the Hotkeys view is its own
    CHECK(manual_heading() == "builder.build-realize -- shift+b"); // ...and the `?` follows it

    b.r.press_cell(marks.gear_x, marks.y);
    choose_context(b, "workshop.hotkeys-keys");
    CHECK(b.r.session().hotkeys_keys == KeySpelling::kShort);
    b.focus();
    CHECK(hotkeys() == "B");
    CHECK(manual_heading() == "builder.build-realize -- B");
    CHECK(band().find("shift+b load after build") != std::string::npos); // the band kept its own

    // BOTH ARE THE WEAVER'S STATED PREFERENCES, saved apart.
    const std::string saved = slurp(b.r.host.prefs_path);
    CHECK(saved.find("\"band_keys\":\"full\"") != std::string::npos);
    CHECK(saved.find("\"hotkeys_keys\":\"short\"") != std::string::npos);
}

TEST_CASE("WL-DESK-17: the manual key asks for the manual of the pane holding the keys; the Manual "
          "keeps no words once closed, and a pane reloaded in place shows its new words at the "
          "next `?`") {
    BuilderRig b("bld-help-reload");
    b.tool->catalog = catalog_of({{"tally", "tally"}});
    b.open(160, 48, false, true, true, false, /*with_desktop=*/true);
    // THE KEY, while the Builder holds the keys: a chord above every mode, which the pane does not
    // keep as it keeps the keys it did not declare.
    b.r.key(input::scan::kF, input::mod::kCtrl);
    REQUIRE(manual_kind(b) != kNoPaneKind);
    std::vector<std::string> lines = read_manual(b);
    CHECK(has_line(lines, "builder.build -- b"));

    // CLOSED WITH ITS OWN KEY, then put back on the desk by the host, no `?` pressed: it shows no
    // word of the Builder's, because nothing kept them.
    b.r.key(input::scan::kEscape);
    CHECK_FALSE(on_desk(b, manual_kind(b)));
    b.r.pick(PaneRef{kDesktopRole, dp::kManualPane});
    REQUIRE(on_desk(b, manual_kind(b)));
    std::vector<std::string> shown = manual_shown(b);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0].rfind("MANUAL -- press the ?", 0) == 0);
    for (const std::string& line : shown) {
        CHECK(line.find("builder") == std::string::npos);
    }
    // ...and the key with no pane holding the keys names none, and says so.
    b.unfocus();
    b.r.key(input::scan::kF, input::mod::kCtrl);
    shown = manual_shown(b);
    REQUIRE(shown.size() >= 2);
    CHECK(shown[1].find("no pane was named") != std::string::npos);

    // THE BUILDER RELOADED IN PLACE WITH OTHER WORDS: its next `?` shows only those.
    b.r.enqueue_reload(pane::kBuilderPaneStem, WORKSHOP_SO_BUILDER_PANE_REWORDED);
    b.r.bus.drain_until_idle();
    REQUIRE(b.r.load_refusals.empty());
    b.r.extent(160, 48);
    const HeaderMarks marks = header_marks(b, b.kind);
    REQUIRE(marks.row.find(kChromeMarks) != std::string::npos);
    b.r.press_cell(marks.manual_x, marks.y);
    lines = read_manual(b);
    bool reworded = false;
    for (const std::string& line : lines) {
        reworded = reworded || line.find("reworded") != std::string::npos;
        CHECK(line.rfind("builder.build -- ", 0) != 0);
    }
    CHECK(reworded);
}
