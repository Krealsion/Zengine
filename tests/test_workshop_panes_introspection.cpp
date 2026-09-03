// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — the external pane seam, from both sides.
// 
//
// An office authors the pane and Workshop grants the room (WP-0), and a maker presses a
// row inside that room and the pane says which entry that was (SEL-0). Both halves are
// driven through the REAL weave on a REAL bus against REAL loaded artifacts — a fixture
// office built here, and two products a maker actually runs — because the claim is about
// the real ABI and the real load path, and a mock loader would prove nothing about
// either.
//
// The rig every case here starts from is `PaneRig` in `workshop_support.hpp`: it is
// shared because a Workshop with a real external pane in it is what the geometry, the
// persistence and the interaction suites need too.
//
// FOUR SOURCES, ONE SUITE, AND THE BOUNDARIES ARE THE FILE'S OWN. `workshop_panes` is
// one CTest entry running one binary; since QR-13 its cases live in five translation
// units, cut along the headings this material already had:
//
//   _seam.cpp           the protocol and the provider -- what an office may offer, who
//                       may speak for it, how Workshop discovers it, the room it grants,
//                       what it retains, and how a pane ends
//   _window.cpp         where the pane SITS -- the authored window, order and recovery,
//                       the units a maker reads and authors, the two arrangement scopes,
//                       and the one graphical boundary
//   _input.cpp          the maker's hand crossing the seam -- a press that names a row,
//                       and the keyboard that reaches a pane
//   _introspection.cpp  the resolved arrangement and the power stack, as two more panes
//   _sampling.cpp       the live seam -- browsing runs nothing, sampling runs exactly one
//
// A NEW CASE GOES TO THE FILE WHOSE SUBJECT IT IS ABOUT. The cut is a reading boundary
// first and an object-format bound second: one MinGW Debug object could no longer name
// all of these instantiations (tests/CMakeLists.txt, QR-13).
//
// THIS FILE OWNS: the shipped introspection panes.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// INTR-1 — the resolved arrangement and the power stack, as two more panes
// ============================================================================
//
// THREE TIERS AGAIN, AND THE SPLIT IS INTR-0'S. The first asks what a projection
// MEANS -- pure functions over a value, no bus, no library, no Workshop -- because
// "every displayed fact is true" is a statement about a projection. The second drives
// the real `zengine-introspection` library through the real pane seam over a real
// authored arrangement and a real `op::Catalog`, because "the fact has an
// authoritative owner" is a statement about a path. The third reads Workshop's own
// source, because "the host knows nothing about these panes" is a statement about a
// file and every rig here would stay green if it stopped being true.
//
// ⚠ THE PANE TIER NEVER LOADS THE TIMER. `PaneRig` pumps to EMPTY and a live Timer
// re-arms its own beat inside its own handler. The arrangement facts that need a
// provider+weave artifact -- the Timer appearing ONCE, the operator handoff outcome --
// are proved in `test_workshop_load.cpp`, whose rigs drain in bounded turns.

// ---- Tier one: what a projection means --------------------------------------------

TEST_CASE("INTR-1: the arrangement's summary is DERIVED from the rows it is printed over") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    const std::vector<surface::SurfaceTextRow> rows = intro::project_arrangement(said, 40, 80);
    REQUIRE_FALSE(rows.empty());

    // EXACT CARDINALITY, NEVER A PERCENTAGE. Four artifacts, all performed, two of
    // which mounted a provider and three of which loaded a weave -- and every one of
    // those numbers is counted over the very list below it, so the summary and the
    // rows cannot come to disagree.
    CHECK(rows[0].text == "4 of 4 artifacts resolved -- 2 providers, 3 weaves");
    CHECK(rows[0].role == surface::role::kAccent);
    CHECK(row_with(rows, "%") == -1);
    CHECK(row_with(rows, "100") == -1);
}

TEST_CASE("INTR-1: a partial arrangement cannot read as a complete one") {
    ws::ResolvedArrangement said = shaped_arrangement();
    said.artifacts[2].state = ws::kRefusedToken;
    said.artifacts[2].provider.clear();
    said.artifacts[2].weave = 0;
    said.artifacts[3].state = ws::kAuthoredToken;
    said.artifacts[3].weave = 0;
    const std::vector<surface::SurfaceTextRow> rows = intro::project_arrangement(said, 40, 80);
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0].text == "2 of 4 artifacts resolved -- 1 providers, 1 weaves");
    // ...and the two unresolved rows say so where a maker reads them, IN TWO DIFFERENT
    // SENTENCES (BOOT-0). One artifact refused and one was never reached; before
    // realization had a persistent owner both were `performed = false` and both read
    // `(not reached)`, which told a maker the wrong story about which one broke.
    const std::int64_t refused = row_with(rows, intro::kRefusedRow);
    REQUIRE(refused >= 0);
    const std::int64_t untried = row_with(rows, intro::kNotReached);
    REQUIRE(untried >= 0);
    CHECK(refused != untried);
    // AND ONLY ONE OF THEM IS AN ALERT. `kAlert` is "something the maker must see"; a
    // row nothing has reached yet in a project that is still coming up is not that.
    CHECK(rows[static_cast<std::size_t>(refused)].role == surface::role::kAlert);
    CHECK(rows[static_cast<std::size_t>(untried)].role == surface::role::kMuted);
}

TEST_CASE("BOOT-0: a project still coming up shows LOADING, and it is not an alert") {
    // ⭐ THE STATE THAT COULD NOT EXIST BEFORE. Realization used to finish inside a
    // single stack frame before any pane could be mounted, so no maker could ever see a
    // row mid-flight. It proceeds through ordinary deliveries now, and this is what the
    // pane says while it does.
    ws::ResolvedArrangement said = shaped_arrangement();
    said.artifacts[1].state = ws::kLoadingToken;
    said.artifacts[1].provider.clear();
    said.artifacts[1].weave = 0;
    said.artifacts[2].state = ws::kAuthoredToken;
    said.artifacts[2].provider.clear();
    said.artifacts[2].weave = 0;
    said.artifacts[3].state = ws::kAuthoredToken;
    said.artifacts[3].weave = 0;
    const std::vector<surface::SurfaceTextRow> rows = intro::project_arrangement(said, 40, 80);
    REQUIRE_FALSE(rows.empty());

    // EXACT COUNTS, AND NOT A PERCENTAGE OR A BAR. One of four has resolved; the rest
    // are three different states and the heading claims nothing about them.
    CHECK(rows[0].text == "1 of 4 artifacts resolved -- 1 providers, 0 weaves");
    CHECK(row_with(rows, "%") == -1);
    const std::int64_t loading = row_with(rows, intro::kLoadingNow);
    REQUIRE(loading >= 0);
    CHECK(rows[static_cast<std::size_t>(loading)].role == surface::role::kMuted);
    // ...AND NOTHING SAYS REFUSED, because nothing refused.
    CHECK(row_with(rows, intro::kRefusedRow) == -1);
}

TEST_CASE("INTR-1: AUTHORED and RESOLVED are two labelled rows, and never one") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_arrangement(shaped_arrangement(), 40, 80);
    const std::vector<std::string> text = texts_of(rows);

    // THE TIMER'S BLOCK, WHOLE: a stem, what a person asked for, and two resolved
    // participations -- so `zengine.timer` the ROLE and `zengine.timer` the PROVIDER
    // IDENTITY, which read alike, are never on one row where a maker could take them
    // for one fact.
    const std::int64_t stem = row_with(rows, "zengine-timer");
    REQUIRE(stem >= 0);
    const std::size_t at = static_cast<std::size_t>(stem);
    REQUIRE(at + 3 < text.size());
    CHECK(text[at] == "  zengine-timer");
    CHECK(text[at + 1] == "    authored  provider normal, weave zengine.timer");
    CHECK(text[at + 2] == "    resolved  provider zengine.timer, 1 power");
    CHECK(text[at + 3] == "    resolved  weave #7, operator host offered");

    // A RESOLVED PROVIDER IDENTITY IS NEVER PRESENTED AS SOMETHING THE PLAN AUTHORED,
    // and a minted WeaveId is never presented as durable intent: neither appears on an
    // `authored` row anywhere in the projection.
    for (const std::string& row : text) {
        if (row.find("    authored ") == 0) {
            CHECK(row.find("#") == std::string::npos);
            CHECK(row.find("power") == std::string::npos);
        }
    }
}

TEST_CASE("INTR-1: one artifact is ONE block, however many surfaces it participates as") {
    const std::vector<std::string> text =
        texts_of(intro::project_arrangement(shaped_arrangement(), 40, 80));
    std::size_t stems = 0;
    for (const std::string& row : text) {
        stems += row == "  zengine-timer" ? 1u : 0u;
    }
    // LOAD-0'S CENTRAL RESULT, IN PRESENTATION. Two participations, one row naming it.
    CHECK(stems == 1);
}

TEST_CASE("INTR-1: a provider-only artifact is visible and wears no weave") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_arrangement(shaped_arrangement(), 40, 80);
    const std::vector<std::string> text = texts_of(rows);
    const std::int64_t at = row_with(rows, "zengine-operators-basic");
    REQUIRE(at >= 0);
    const std::size_t i = static_cast<std::size_t>(at);
    REQUIRE(i + 3 < text.size());
    CHECK(text[i] == "  zengine-operators-basic");
    CHECK(text[i + 1] == "    authored  provider normal");
    CHECK(text[i + 2] == "    resolved  provider zengine.operators.basic, 2 powers");
    // `provider != weave`, ALL THE WAY INTO PRESENTATION. The block ENDS after those
    // three rows: the next one is the next artifact's stem, so there is no WeaveId, no
    // role and -- the trap this case is really about -- no operator-host outcome for a
    // load that never happened.
    CHECK(text[i + 3].rfind("    ", 0) != 0);
    CHECK(text[i + 3].find("weave") == std::string::npos);
}

TEST_CASE("INTR-1: a weave-only artifact is visible and wears no provider") {
    const std::vector<surface::SurfaceTextRow> rows =
        intro::project_arrangement(shaped_arrangement(), 40, 80);
    const std::vector<std::string> text = texts_of(rows);
    const std::int64_t at = row_with(rows, "zengine-composer");
    REQUIRE(at >= 0);
    const std::size_t i = static_cast<std::size_t>(at);
    REQUIRE(i + 2 < text.size());
    CHECK(text[i + 1] == "    authored  weave zengine.composer");
    CHECK(text[i + 2] == "    resolved  weave #9, operator host not-a-consumer");
    CHECK(text[i + 2].find("provider") == std::string::npos);
    // ...and the block is TWO rows under its stem, so nothing was invented for a
    // surface the plan never asked for.
    CHECK((i + 3 == text.size() || text[i + 3].rfind("    ", 0) != 0));
}

TEST_CASE("INTR-1: an artifact BLOCK is shown whole or counted, never half") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    // THE ONE-ROW FLOOR FIRST, because it is the budget at which the rule below does
    // not apply and the accounting still holds: there is no list, no note and no
    // marker -- and the HEADING states the population, so nothing is hidden without
    // being counted even there.
    const std::vector<surface::SurfaceTextRow> floor = intro::project_arrangement(said, 1, 60);
    REQUIRE(floor.size() == 1);
    CHECK(floor[0].text.rfind("4 of 4 artifacts", 0) == 0);

    // ...AND FROM TWO ROWS UP, SHOWING PART OF A LIST OBLIGES SAYING HOW MUCH WAS HIDDEN.
    for (std::int64_t rows = 2; rows <= 24; ++rows) {
        const std::vector<surface::SurfaceTextRow> shown =
            intro::project_arrangement(said, rows, 60);
        CHECK(static_cast<std::int64_t>(shown.size()) <= rows);
        const std::vector<std::string> text = texts_of(shown);
        // EVERY STEM ROW IS FOLLOWED BY ITS OWN `authored` ROW. A block cut in half
        // would leave a stem naming nothing, which is a different and wrong answer
        // rather than a shorter one.
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i].rfind("  zengine-", 0) == 0 && text[i].rfind("    ", 0) != 0) {
                REQUIRE(i + 1 < text.size());
                CHECK(text[i + 1].rfind("    authored  ", 0) == 0);
            }
        }
        // AND WHAT IS NOT SHOWN IS COUNTED. Nothing is hidden in silence at any budget.
        std::size_t blocks = 0;
        for (const std::string& row : text) {
            blocks += (row.rfind("  zengine-", 0) == 0 && row.rfind("    ", 0) != 0) ? 1u : 0u;
        }
        if (blocks < said.artifacts.size()) {
            const std::int64_t marker = row_with(shown, "more");
            REQUIRE_MESSAGE(marker >= 0, "rows=", rows, " showed ", blocks, " and said nothing");
            CHECK(text[static_cast<std::size_t>(marker)] ==
                  "  ... " + std::to_string(said.artifacts.size() - blocks) + " more");
        }
    }
}

TEST_CASE("INTR-1: the population bound is reserved before the list gets a second row") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    // AT EVERY BUDGET THAT SHOWS A LIST AT ALL, the sentence saying what these rows are
    // NOT is on the canvas -- INTR-0's reservation argument, in a third place. A maker
    // reading `4 of 4 artifacts` beside a running Builder would otherwise be right to
    // conclude the Builder is not running.
    for (std::int64_t rows = 3; rows <= 30; ++rows) {
        const std::vector<surface::SurfaceTextRow> shown =
            intro::project_arrangement(said, rows, 70);
        CHECK_MESSAGE(row_with(shown, intro::kNotAuthored) >= 0, "rows=", rows);
    }
    // ...AND THE LIST'S FIRST ROW OUTRANKS THE CAVEAT, which is the other half of the
    // same rule and is why the sweep starts at three. At a TWO-row body there is one
    // row left after the heading and it goes to the list -- a pane showing only its own
    // small print has stopped being a view of anything. What that one row can carry
    // here is not a block, so it carries the count of what is hidden.
    const std::vector<surface::SurfaceTextRow> two = intro::project_arrangement(said, 2, 70);
    REQUIRE(two.size() == 2);
    CHECK(two[1].text == "  ... 4 more");
    // ...and at a ONE-row body there is no list and no note, and the population is
    // still stated: the heading is the floor of the accounting.
    const std::vector<surface::SurfaceTextRow> floor = intro::project_arrangement(said, 1, 70);
    REQUIRE(floor.size() == 1);
    CHECK(floor[0].text.rfind("4 of 4 artifacts", 0) == 0);
}

TEST_CASE("INTR-1: the plan line is spent only out of genuine slack") {
    const ws::ResolvedArrangement said = shaped_arrangement();
    // A pane that had to window its own project spends that row on the project.
    const std::vector<surface::SurfaceTextRow> tight = intro::project_arrangement(said, 8, 70);
    CHECK(row_with(tight, "plan: ") == -1);
    CHECK(row_with(tight, "more") >= 0);
    // ...and one with room over says where the authored rows came from.
    const std::vector<surface::SurfaceTextRow> roomy = intro::project_arrangement(said, 30, 70);
    const std::int64_t at = row_with(roomy, "plan: default-load-plan.json");
    REQUIRE(at >= 0);
    CHECK(roomy[static_cast<std::size_t>(at)].role == surface::role::kMuted);
}

TEST_CASE("INTR-1: every arrangement projection fits the room it was given") {
    // THE WHOLE DOMAIN, because being exactly inside the grant is this function's
    // obligation rather than a courtesy: Workshop refuses an over-budget update WHOLE.
    const ws::ResolvedArrangement said = shaped_arrangement();
    for (std::int64_t rows = 0; rows <= 26; ++rows) {
        for (std::int64_t columns = 0; columns <= 90; columns += 3) {
            const std::vector<surface::SurfaceTextRow> shown =
                intro::project_arrangement(said, rows, columns);
            REQUIRE(static_cast<std::int64_t>(shown.size()) <= (rows < 0 ? 0 : rows));
            for (const surface::SurfaceTextRow& row : shown) {
                REQUIRE(static_cast<std::int64_t>(row.text.size()) <= columns);
            }
        }
    }
}

TEST_CASE("INTR-1: an empty arrangement is an observed zero, and says what it is not") {
    ws::ResolvedArrangement empty;
    const std::vector<surface::SurfaceTextRow> shown = intro::project_arrangement(empty, 10, 60);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0].text == "0 of 0 artifacts resolved -- 0 providers, 0 weaves");
    CHECK(row_with(shown, intro::kNotAuthored) >= 0);
}

// ---- Tier one, the powers half: two views over one reading (SOURCE-1) -------------
//
// EVERYTHING HERE IS ASKED OF A VALUE. `PowersUi` holds one reading and the maker's
// own state; `project_powers_ui` answers what a maker would SEE and what each place
// would MEAN. There is no bus, no catalog, no weave and no evaluator in this tier --
// which is also the structural half of the zero-evaluation claim: these cases could
// not run a Source if they tried, because the code under test cannot name one.

namespace {

std::vector<std::string> powers_text(const intro::PowersUi& ui, std::int64_t rows,
                                     std::int64_t columns) {
    return texts_of(intro::project_powers_ui(ui, rows, columns).rows);
}

/// The identities the projection actually drew as entry rows, in row order -- read
/// back off the row map rather than off the text, because the map is what a press
/// spends and a case that read the text would be checking a different function.
std::vector<std::string> listed(const intro::PowersView& view) {
    std::vector<std::string> out;
    for (const intro::PowersSpan& s : view.spans) {
        if (s.control == intro::powers_control::kEntry) {
            out.push_back(s.identity);
        }
    }
    return out;
}

std::vector<std::string> names_of(const std::vector<const ws::PowerStack*>& list) {
    std::vector<std::string> out;
    for (const ws::PowerStack* p : list) {
        out.push_back(p->power);
    }
    return out;
}

/// Where a control was drawn, or {-1,-1}. Cases press through this so that every
/// pointer assertion spends the SAME map the projection published.
std::pair<std::int64_t, std::int64_t> place_of(const intro::PowersView& view,
                                               std::int64_t control) {
    for (const intro::PowersSpan& s : view.spans) {
        if (s.control == control) {
            return {s.row, s.first};
        }
    }
    return {-1, -1};
}

} // namespace

TEST_CASE("SOURCE-1: view membership is the exterior contract, never the identity's spelling") {
    // ⭐ THE FALSIFIER THE WHOLE DERIVATION RESTS ON. `source.looks.like.one` takes
    // arguments and `math.max` does not, so an implementation that read a view off an
    // identity's spelling would put both in the wrong list -- and every other case in
    // this file would stay green while it did.
    intro::PowersUi ui = showing(four_cells());
    ui.view = intro::powers_view::kSources;
    CHECK(names_of(intro::filtered_of(ui)) ==
          std::vector<std::string>{"math.max", "zengine.recipes.catalog"});
    ui.view = intro::powers_view::kOperators;
    CHECK(names_of(intro::filtered_of(ui)) ==
          std::vector<std::string>{"logic.select_int", "source.looks.like.one"});

    // AND THE CLASSIFICATION IS READ AND NEVER AUTHORED. Nothing a contributor writes
    // says which list it belongs in: the wire shape carries `source`, which is
    // `op::is_source` read off the definition the host resolves through, and there is
    // no second field anywhere for a registration to disagree with.
    const std::shared_ptr<const loom::Schema> shape = loom::schema_of<ws::PowerContribution>();
    REQUIRE(shape != nullptr);
    for (const loom::Field& f : shape->fields()) {
        CHECK(f.name != "kind");
        CHECK(f.name != "species");
        CHECK(f.name != "declared_source");
    }
}

TEST_CASE("SOURCE-1: all four Source x Composite cells are legal and independent") {
    // THE FOUR COMBINATIONS ARE ARRANGED AND ALL FOUR ARE SHOWN. Source/Operator is
    // the exterior contract of the IDENTITY; composite is the construction of the
    // ACTIVE CONTRIBUTION; neither answers the other.
    intro::PowersUi ui = showing(four_cells());
    struct Cell {
        const char* identity;
        bool source;
        bool composite;
    };
    const std::vector<Cell> want{{"logic.select_int", false, false},
                                 {"math.max", true, false},
                                 {"source.looks.like.one", false, true},
                                 {"zengine.recipes.catalog", true, true}};
    for (const Cell& c : want) {
        bool found = false;
        for (const ws::PowerStack& p : ui.reading.powers) {
            if (p.power != c.identity) {
                continue;
            }
            found = true;
            CHECK_MESSAGE(intro::is_source_power(p) == c.source, c.identity);
            CHECK_MESSAGE(intro::is_composite_power(p) == c.composite, c.identity);
        }
        CHECK_MESSAGE(found, c.identity);
    }

    // AND THE COMPOSITE FILTER CROSSES BOTH VIEWS, taking one cell out of each.
    ui.composite_only = true;
    ui.view = intro::powers_view::kSources;
    CHECK(names_of(intro::filtered_of(ui)) ==
          std::vector<std::string>{"zengine.recipes.catalog"});
    ui.view = intro::powers_view::kOperators;
    CHECK(names_of(intro::filtered_of(ui)) == std::vector<std::string>{"source.looks.like.one"});

    // NO OPENABILITY IS CLAIMED ANYWHERE. The badge says construction and nothing else;
    // there is no Flow in this build and no row that offers one.
    const std::vector<std::string> shown = powers_text(showing(four_cells()), 20, 70);
    for (const char* absent : {"Flow", "flow", "Open in", "graph", "deconstruct"}) {
        CHECK_MESSAGE(row_with_text(shown, absent) == -1, absent);
    }
}

TEST_CASE("SOURCE-1: the composite badge and filter read the ACTIVE contribution") {
    // ⭐ A BURIED CONTRIBUTION IS NOT THE ANSWER. A native covered by a composite is a
    // composite power now; a composite covered by a native is not. An implementation
    // reading `contributions[0]` would answer both backwards.
    ws::ResolvedPowers said;
    said.providers = {"a", "b"};
    said.powers.push_back(power_of("native.under.composite",
                                   {source_from("a", /*composite=*/false),
                                    source_from("b", /*composite=*/true)}));
    said.powers.push_back(power_of("composite.under.native",
                                   {source_from("a", /*composite=*/true),
                                    source_from("b", /*composite=*/false)}));
    intro::PowersUi ui = showing(said);
    CHECK(intro::is_composite_power(ui.reading.powers[0]));
    CHECK_FALSE(intro::is_composite_power(ui.reading.powers[1]));

    ui.composite_only = true;
    CHECK(names_of(intro::filtered_of(ui)) ==
          std::vector<std::string>{"native.under.composite"});

    // AND THE ROW A MAKER READS AGREES WITH THE FILTER, which is the half a badge
    // computed somewhere else would get wrong silently.
    const std::vector<std::string> shown = powers_text(showing(said), 20, 70);
    const std::int64_t marked = row_with_text(shown, "native.under.composite");
    const std::int64_t plain = row_with_text(shown, "composite.under.native");
    REQUIRE(marked >= 0);
    REQUIRE(plain >= 0);
    CHECK(shown[static_cast<std::size_t>(marked)].find("(composite)") != std::string::npos);
    CHECK(shown[static_cast<std::size_t>(plain)].find("(composite)") == std::string::npos);
}

TEST_CASE("SOURCE-1: an overlay changing construction moves the badge and not the view") {
    // THE NEXT READING IS THE BEAT, and what it changes is exactly one of the two
    // independent facts: the identity is a Source before and after, and the badge
    // follows the contribution that arrived on top.
    ws::ResolvedPowers before;
    before.providers = {"host"};
    before.powers.push_back(power_of("thing", {source_from("host", /*composite=*/false)}));
    intro::PowersUi ui = showing(before);
    ui.select("thing");
    CHECK(intro::is_source_power(ui.reading.powers[0]));
    CHECK_FALSE(intro::is_composite_power(ui.reading.powers[0]));

    ws::ResolvedPowers after;
    after.providers = {"host", "overlay"};
    after.powers.push_back(power_of("thing", {source_from("host", /*composite=*/false),
                                              source_from("overlay", /*composite=*/true)}));
    ui.reading = after;
    intro::revalidate(ui);
    CHECK(intro::is_source_power(ui.reading.powers[0]));    // the contract did not move
    CHECK(intro::is_composite_power(ui.reading.powers[0])); // the construction did
    CHECK(ui.selected() == "thing");                        // and the maker kept their place
    CHECK(names_of(intro::filtered_of(ui)) == std::vector<std::string>{"thing"});
}

TEST_CASE("SOURCE-1: search is a case-insensitive ASCII substring that filters, never ranks") {
    CHECK(intro::matches_query("math.max", ""));   // empty selects all
    CHECK(intro::matches_query("math.max", "MATH"));
    CHECK(intro::matches_query("MATH.MAX", "math"));
    CHECK(intro::matches_query("math.max", ".ma"));
    CHECK(intro::matches_query("math.max", "math.max"));
    CHECK_FALSE(intro::matches_query("math.max", "maths"));
    CHECK_FALSE(intro::matches_query("max", "math.max")); // longer than the identity

    // AND IT FILTERS RATHER THAN RANKING. `src.01` matches later in its name than
    // `src.10` does, and what comes back is still the catalog's own order.
    intro::PowersUi ui = showing(many_sources(12));
    ui.query.type("1");
    const std::vector<std::string> got = names_of(intro::filtered_of(ui));
    REQUIRE(got.size() == 3);
    CHECK(got[0] == "src.01");
    CHECK(got[1] == "src.10");
    CHECK(got[2] == "src.11");
}

TEST_CASE("SOURCE-1: bytes at or above 0x80 compare exactly") {
    // NO LOCALE AND NO UNICODE FOLDING. This pane's matching is ASCII, and a byte
    // outside it is compared for what it is rather than for what some table thinks it
    // means.
    const std::string high = "na\xC3\xAFve.thing";
    CHECK(intro::matches_query(high, "\xC3\xAF"));
    CHECK_FALSE(intro::matches_query(high, "\xC3\x8F")); // the other case, byte-wise
    CHECK(intro::matches_query(high, "NA"));             // ...and the ASCII half still folds
}

TEST_CASE("SOURCE-1: composite-only and the query compose as AND, in both views") {
    intro::PowersUi ui = showing(four_cells());
    ui.view = intro::powers_view::kOperators;
    ui.query.type("o");
    CHECK(names_of(intro::filtered_of(ui)) ==
          std::vector<std::string>{"logic.select_int", "source.looks.like.one"});
    ui.composite_only = true;
    CHECK(names_of(intro::filtered_of(ui)) == std::vector<std::string>{"source.looks.like.one"});
    ui.query.clear();
    ui.query.type("logic");
    CHECK(intro::filtered_of(ui).empty()); // composite AND the query, never either
}

TEST_CASE("SOURCE-1: each view holds its own selected IDENTITY, and switching restores both") {
    intro::PowersUi ui = showing(four_cells());
    ui.view = intro::powers_view::kSources;
    ui.select("zengine.recipes.catalog");
    ui.view = intro::powers_view::kOperators;
    CHECK(ui.selected().empty()); // the other view's place is not this view's
    ui.select("logic.select_int");

    ui.view = intro::powers_view::kSources;
    CHECK(ui.selected() == "zengine.recipes.catalog");
    ui.view = intro::powers_view::kOperators;
    CHECK(ui.selected() == "logic.select_int");

    // AND THE MARK RETURNS WITH THE ROW, in the picture a maker reads.
    ui.view = intro::powers_view::kSources;
    const std::vector<std::string> shown = powers_text(ui, 20, 70);
    const std::int64_t at = row_with_text(shown, "zengine.recipes.catalog");
    REQUIRE(at >= 0);
    CHECK(shown[static_cast<std::size_t>(at)].rfind(intro::kSelectedMark, 0) == 0);
}

TEST_CASE("SOURCE-1: presentation hides a selection; only a fresh reading may clear it") {
    intro::PowersUi ui = showing(four_cells());
    ui.select("math.max");

    // HIDDEN BY A QUERY: held, and unmarked because there is no row to mark.
    ui.query.type("recipes");
    CHECK(ui.selected() == "math.max");
    CHECK(intro::cursor_in(intro::filtered_of(ui), ui.selected()) == -1);
    // ⭐ AND A FRESH READING ARRIVING WHILE THE QUERY HIDES IT DOES NOT CLEAR IT EITHER.
    // The population is what a reading is evidence about; the query is a fact about the
    // maker, and revalidation that consulted it would clear a place because somebody
    // typed three characters.
    intro::revalidate(ui);
    CHECK(ui.selected() == "math.max");
    ui.query.clear();
    CHECK(intro::cursor_in(intro::filtered_of(ui), ui.selected()) == 0);

    // HIDDEN BY THE COMPOSITE FILTER: held.
    ui.composite_only = true;
    CHECK(ui.selected() == "math.max");
    ui.composite_only = false;

    // HIDDEN BY THE WINDOW: held. A short pane is not evidence about a population.
    const intro::PowersView tiny = intro::project_powers_ui(ui, 2, 60);
    CHECK(ui.selected() == "math.max");
    CHECK(static_cast<std::int64_t>(tiny.rows.size()) <= 2);

    // ⭐ AND A FRESH READING THAT NO LONGER HAS IT DOES CLEAR IT -- the only thing that
    // may. Presentation hides; only the population invalidates.
    ws::ResolvedPowers gone = four_cells();
    gone.powers.erase(gone.powers.begin() + 1); // math.max
    ui.reading = gone;
    intro::revalidate(ui);
    CHECK(ui.selected().empty());

    // ...AND A READING THAT STILL HAS IT DOES NOT.
    intro::PowersUi kept = showing(four_cells());
    kept.select("math.max");
    kept.reading = four_cells();
    intro::revalidate(kept);
    CHECK(kept.selected() == "math.max");

    // NOR DOES A READING THAT LOST THE OTHER VIEW'S SELECTION TAKE THIS ONE WITH IT.
    intro::PowersUi both = showing(four_cells());
    both.selected_source = "math.max";
    both.selected_operator = "logic.select_int";
    ws::ResolvedPowers half = four_cells();
    half.powers.erase(half.powers.begin()); // logic.select_int
    both.reading = half;
    intro::revalidate(both);
    CHECK(both.selected_source == "math.max");
    CHECK(both.selected_operator.empty());
}

TEST_CASE("SOURCE-1: the window is derived from the population and the cursor, never stored") {
    // THE THREE RULES: a population that fits is shown whole, the cursor is inside the
    // window, and every omission is counted and spends a row of the same budget.
    CHECK(intro::powers_window(3, 0, 8).count == 3);
    CHECK(intro::powers_window(3, 0, 8).before == 0);
    CHECK(intro::powers_window(3, 0, 8).after == 0);

    const intro::PowersWindow head = intro::powers_window(20, 0, 5);
    CHECK(head.first == 0);
    CHECK(head.count == 4); // one row of the five is the marker's
    CHECK(head.after == 16);

    const intro::PowersWindow tail = intro::powers_window(20, 19, 5);
    CHECK(tail.first + tail.count == 20);
    CHECK(tail.before == 16);
    CHECK(tail.after == 0);

    for (std::int64_t cursor = 0; cursor < 20; ++cursor) {
        const intro::PowersWindow w = intro::powers_window(20, cursor, 5);
        CHECK_MESSAGE(cursor >= w.first, "cursor=", cursor);
        CHECK_MESSAGE(cursor < w.first + w.count, "cursor=", cursor);
        CHECK(w.before + w.count + w.after == 20);
    }
    // TOTAL where nobody can spend it: a zero-row budget hides the population and says
    // so rather than answering with numbers that do not add up.
    CHECK(intro::powers_window(20, 3, 0).after == 20);
    CHECK(intro::powers_window(20, 3, 1).after == 20);
    CHECK(intro::powers_window(0, 0, 8).count == 0);

    // ⭐ AND THE WINDOW MOVES WITH THE CURSOR WITHOUT ANYTHING BEING REMEMBERED. Two
    // projections of the same state are identical; a projection after a cursor move
    // shows the row the cursor is on, whichever way it came.
    intro::PowersUi ui = showing(many_sources(40));
    ui.select("src.30");
    const std::vector<std::string> once = powers_text(ui, 8, 60);
    CHECK(once == powers_text(ui, 8, 60));
    CHECK(row_with_text(once, "src.30") >= 0);
    ui.select("src.00");
    const std::vector<std::string> home = powers_text(ui, 8, 60);
    CHECK(row_with_text(home, "src.00") >= 0);
    CHECK(row_with_text(home, "src.30") == -1);
    ui.select("src.30");
    CHECK(powers_text(ui, 8, 60) == once); // no scroll offset survived the round trip
}

TEST_CASE("SOURCE-1: Up and Down walk the visible list, and begin at its head when hidden") {
    intro::PowersUi ui = showing(many_sources(6));
    CHECK(ui.selected().empty());
    intro::move_cursor(ui, +1);
    CHECK(ui.selected() == "src.00"); // no cursor: begin at the beginning
    intro::move_cursor(ui, +1);
    CHECK(ui.selected() == "src.01");
    intro::move_cursor(ui, -1);
    CHECK(ui.selected() == "src.00");
    intro::move_cursor(ui, -1);
    CHECK(ui.selected() == "src.00"); // bounded at the head

    for (int i = 0; i < 10; ++i) {
        intro::move_cursor(ui, +1);
    }
    CHECK(ui.selected() == "src.05"); // ...and at the tail

    // A HIDDEN SELECTION IS NOT PROJECTED INTO THE LIST TO BE LEFT. The identity is
    // still held; the walk begins where the maker can actually see.
    ui.select("src.03");
    ui.query.type("src.05");
    CHECK(ui.selected() == "src.03");
    intro::move_cursor(ui, +1);
    CHECK(ui.selected() == "src.05");

    // AND AN EMPTY LIST MOVES NOTHING AT ALL.
    intro::PowersUi none = showing(ws::ResolvedPowers{});
    intro::move_cursor(none, +1);
    CHECK(none.selected().empty());
}

TEST_CASE("SOURCE-1: the position marker counts the list the maker is navigating") {
    intro::PowersUi ui = showing(many_sources(12));
    CHECK(intro::position_marker(-1, 12) == "-/12"); // nothing selected is not position zero
    CHECK(intro::position_marker(0, 12) == "1/12");  // and the first is one, not nought
    CHECK(intro::position_marker(11, 12) == "12/12");

    ui.select("src.05");
    intro::PowersView view = intro::project_powers_ui(ui, 20, 70);
    CHECK(view.population == 12);
    CHECK(view.cursor == 5);
    CHECK(row_with_text(texts_of(view.rows), "6/12") == 0);

    // ⭐ THE DENOMINATOR FOLLOWS THE FILTER, because it describes the list actually
    // being navigated. A total from the unfiltered reading over a cursor from the
    // filtered one is never wrong on any single row and always wrong as a sentence.
    ui.query.type("src.0");
    view = intro::project_powers_ui(ui, 20, 70);
    CHECK(view.population == 10);
    CHECK(view.cursor == 5);
    CHECK(row_with_text(texts_of(view.rows), "6/10") == 0);
}

TEST_CASE("SOURCE-1: an absent view and a filtered-away one are different sentences") {
    // A VIEW WITH NOTHING IN IT.
    intro::PowersUi empty = showing(four_cells());
    empty.reading.powers.erase(empty.reading.powers.begin() + 3); // zengine.recipes.catalog
    empty.reading.powers.erase(empty.reading.powers.begin() + 1); // math.max
    empty.view = intro::powers_view::kSources;
    CHECK(row_with_text(powers_text(empty, 8, 60), intro::kNoSourcesHere) >= 0);

    intro::PowersUi no_ops = showing(four_cells());
    no_ops.reading.powers.erase(no_ops.reading.powers.begin() + 2); // source.looks.like.one
    no_ops.reading.powers.erase(no_ops.reading.powers.begin());     // logic.select_int
    no_ops.view = intro::powers_view::kOperators;
    CHECK(row_with_text(powers_text(no_ops, 8, 60), intro::kNoOperatorsHere) >= 0);

    // ⭐ AND A VIEW THE MAKER FILTERED AWAY SAYS SO AND COUNTS WHAT IT IS HIDING, so an
    // empty pane can never read as an empty system.
    intro::PowersUi hidden = showing(four_cells());
    hidden.view = intro::powers_view::kSources;
    hidden.query.type("nothing-matches-this");
    const std::vector<std::string> said = powers_text(hidden, 8, 60);
    CHECK(row_with_text(said, intro::kNoSourcesHere) == -1);
    CHECK(row_with_text(said, "2 sources here") >= 0);
    CHECK(row_with_text(said, "hidden by the current filter") >= 0);
}

TEST_CASE("SOURCE-1: every projection fits the room it was given") {
    std::vector<intro::PowersUi> arrangements;
    arrangements.push_back(intro::PowersUi{});             // never read: no reading at all
    arrangements.push_back(showing(ws::ResolvedPowers{})); // an observed empty catalog
    arrangements.push_back(showing(four_cells()));
    arrangements.push_back(showing(many_sources(40)));
    {
        intro::PowersUi busy = showing(four_cells());
        busy.select("zengine.recipes.catalog");
        busy.composite_only = true;
        busy.query.type("cat");
        busy.sample.present = true;
        busy.sample.identity = "zengine.recipes.catalog";
        busy.sample.ok = true;
        busy.sample.lines = {"zengine.RecipeCatalog v1", "  catalog",
                             "    source   \"/a/very/long/path/that/will/not/fit/at/all\"",
                             "    recipes  6"};
        arrangements.push_back(busy);
        intro::PowersUi refused = busy;
        refused.sample.ok = false;
        refused.sample.lines.clear();
        refused.sample.reason =
            "nothing supplies 'zengine.recipes.catalog' in this catalog, and a sample "
            "cannot invent one";
        arrangements.push_back(refused);
    }

    for (const intro::PowersUi& ui : arrangements) {
        for (std::int64_t rows = 0; rows <= 24; ++rows) {
            for (std::int64_t columns = 0; columns <= 90; columns += 3) {
                const intro::PowersView view = intro::project_powers_ui(ui, rows, columns);
                REQUIRE(static_cast<std::int64_t>(view.rows.size()) <= rows);
                for (const surface::SurfaceTextRow& row : view.rows) {
                    REQUIRE(static_cast<std::int64_t>(row.text.size()) <= columns);
                }
                // AND THE MAP IS PARALLEL TO WHAT WAS DRAWN, always: a span naming a row
                // that does not exist is a press resolved against a picture nobody
                // published.
                for (const intro::PowersSpan& s : view.spans) {
                    REQUIRE(s.row >= 0);
                    REQUIRE(s.row < static_cast<std::int64_t>(view.rows.size()));
                    REQUIRE(s.first >= 0);
                    REQUIRE(s.last < columns);
                }
            }
        }
    }
}

TEST_CASE("SOURCE-1: the two measured default budgets stay useful") {
    // THE SHIPPED TERMINAL PANE IS EIGHT PROSE ROWS AND THE SHIPPED GRAPHICAL ONE IS
    // FOUR. Every composition decision was made against those two numbers, so they are
    // asserted rather than assumed.
    intro::PowersUi ui = showing(four_cells());
    ui.select("math.max");
    ui.sample.present = true;
    ui.sample.identity = "math.max";
    ui.sample.ok = true;
    ui.sample.lines = {"zengine.MaxSoFar v1", "  value  7"};

    const std::vector<std::pair<std::int64_t, std::int64_t>> budgets{{8, 48}, {4, 71}};
    for (const std::pair<std::int64_t, std::int64_t>& budget : budgets) {
        const intro::PowersView view =
            intro::project_powers_ui(ui, budget.first, budget.second);
        const std::vector<std::string> shown = texts_of(view.rows);
        REQUIRE_FALSE(shown.empty());
        // THE ACTIVE VIEW IS SAYABLE AT BOTH, and it is a WORD rather than an ink.
        CHECK_MESSAGE(shown[0].find("[Sources]") != std::string::npos, budget.second);
        // ...AND SO IS WHERE THE MAKER IS.
        CHECK_MESSAGE(shown[0].find("1/2") != std::string::npos, budget.second);
        // BOTH VIEW CONTROLS ARE REACHABLE BY POINTER AT BOTH BUDGETS.
        CHECK(place_of(view, intro::powers_control::kSources).first == 0);
        CHECK(place_of(view, intro::powers_control::kOperators).first == 0);
        // AND THERE IS A LIST TO NAVIGATE, with the maker's own row in it.
        CHECK_MESSAGE(!listed(view).empty(), budget.second);
        CHECK_MESSAGE(row_with_text(shown, "math.max") >= 0, budget.second);
    }

    // AT FOUR ROWS THE RETAINED SAMPLE STILL ANNOUNCES ITSELF, because its header leads
    // with the tense and counts what it could not show.
    const std::vector<std::string> narrow = powers_text(ui, 4, 71);
    CHECK(row_with_text(narrow, intro::kSampledWhenAsked) >= 0);

    // AND A TALLER AUTHORED PANE REVEALS MORE WITH NOTHING HERE EDITED.
    const std::vector<std::string> tall = powers_text(ui, 20, 89);
    CHECK(row_with_text(tall, "yields zengine.MaxSoFar v1") >= 0);
    CHECK(row_with_text(tall, intro::kSampleControl) >= 0);
    CHECK(row_with_text(tall, "  value  7") >= 0);
    CHECK(row_with_text(tall, intro::kHostResolution) >= 0);
    CHECK(row_with_text(tall, intro::kPowersSource) >= 0);
}

TEST_CASE("SOURCE-1: one place means one thing, and the map is the projection read backwards") {
    intro::PowersUi ui = showing(four_cells());
    ui.select("math.max");
    const intro::PowersView view = intro::project_powers_ui(ui, 20, 70);

    // THE CHROME CARRIES THREE SEPARATE CONTROLS ON ONE ROW, and a press resolves to
    // exactly one of them -- which is why the map is columns and not just rows.
    const std::pair<std::int64_t, std::int64_t> sources =
        place_of(view, intro::powers_control::kSources);
    const std::pair<std::int64_t, std::int64_t> operators =
        place_of(view, intro::powers_control::kOperators);
    const std::pair<std::int64_t, std::int64_t> composite =
        place_of(view, intro::powers_control::kComposite);
    REQUIRE(sources.first == 0);
    REQUIRE(operators.first == 0);
    REQUIRE(composite.first == 0);
    CHECK(sources.second < operators.second);
    CHECK(operators.second < composite.second);
    CHECK(intro::target_at(view, 0, sources.second).control == intro::powers_control::kSources);
    CHECK(intro::target_at(view, 0, operators.second).control ==
          intro::powers_control::kOperators);
    CHECK(intro::target_at(view, 0, composite.second).control ==
          intro::powers_control::kComposite);

    // ⭐ AN ENTRY ROW SELECTS, AND SELECTING IS ALL IT DOES. There is no place in this
    // pane where one press is two acts -- which is what keeps a cold pane's FIRST
    // press, the one that also points the keyboard here, from being a hidden double
    // action.
    for (const intro::PowersSpan& s : view.spans) {
        CHECK(s.control != intro::powers_control::kNone);
        if (s.control == intro::powers_control::kEntry) {
            CHECK_FALSE(s.identity.empty());
        }
        for (std::int64_t c = s.first; c <= s.last; ++c) {
            const intro::PowersTarget hit = intro::target_at(view, s.row, c);
            CHECK_MESSAGE(hit.control == s.control, "row=", s.row, " column=", c);
        }
    }

    // AND EVERY OTHER PLACE MEANS NOTHING AT ALL -- the caveat, the census, the source
    // line and an omission marker included.
    const std::int64_t bound = row_with_text(texts_of(view.rows), intro::kHostResolution);
    REQUIRE(bound >= 0);
    CHECK(intro::target_at(view, bound, 0).control == intro::powers_control::kNone);
    CHECK(intro::target_at(view, -1, 0).control == intro::powers_control::kNone);
    CHECK(intro::target_at(view, 9999, 0).control == intro::powers_control::kNone);

    const intro::PowersView windowed =
        intro::project_powers_ui(showing(many_sources(40)), 8, 60);
    const std::int64_t omission = row_with_text(texts_of(windowed.rows), "more below");
    REQUIRE(omission >= 0);
    CHECK(intro::target_at(windowed, omission, 0).control == intro::powers_control::kNone);
}

TEST_CASE("SOURCE-1: a control the width cut is not a target") {
    // ⭐ THE INVERSE MUST AGREE WITH THE PICTURE. A width too narrow for the second view
    // control draws `...` where it would have been, and a press on that mark must not
    // operate a control the maker cannot see.
    const intro::PowersUi ui = showing(four_cells());
    for (std::int64_t columns = 1; columns <= 24; ++columns) {
        const intro::PowersView view = intro::project_powers_ui(ui, 8, columns);
        REQUIRE_FALSE(view.rows.empty());
        const std::string& chrome = view.rows[0].text;
        for (const intro::PowersSpan& s : view.spans) {
            if (s.row != 0) {
                continue;
            }
            REQUIRE(s.last < static_cast<std::int64_t>(chrome.size()));
            const std::string covered =
                chrome.substr(static_cast<std::size_t>(s.first),
                              static_cast<std::size_t>(s.last - s.first + 1));
            CHECK_MESSAGE(covered.find("...") == std::string::npos, "columns=", columns);
        }
    }
}

TEST_CASE("SOURCE-1: the retained sample is history, and says so before it says whose") {
    intro::PowersUi ui = showing(four_cells());
    ui.select("zengine.recipes.catalog");
    ui.sample.present = true;
    ui.sample.identity = "zengine.recipes.catalog";
    ui.sample.ok = true;
    ui.sample.lines = {"zengine.RecipeCatalog v1", "  catalog", "    source   \"/x\"",
                       "    recipes  6"};

    const std::vector<std::string> shown = powers_text(ui, 20, 70);
    const std::int64_t at = row_with_text(shown, intro::kSampledWhenAsked);
    REQUIRE(at >= 0);
    CHECK(shown[static_cast<std::size_t>(at)].find("zengine.recipes.catalog") !=
          std::string::npos);
    CHECK(row_with_text(shown, "    recipes  6") >= 0);

    // ⭐ NO WORDING IMPLIES THE VALUE IS STILL TRUE. `sampled when asked` is the whole
    // claim, and the words that would overstate it appear nowhere.
    for (const char* overclaim : {"current", "live", "latest", "up to date", "refresh"}) {
        CHECK_MESSAGE(row_with_text(shown, overclaim) == -1, overclaim);
    }

    // ⭐ AND THE TENSE SURVIVES A CUT, because `fit` takes the tail and the tail is the
    // identity -- which a maker can recover from the list, unlike the claim.
    for (std::int64_t columns = 22; columns <= 70; ++columns) {
        const std::vector<std::string> cut = powers_text(ui, 20, columns);
        CHECK_MESSAGE(row_with_text(cut, "sampled when asked") >= 0, "columns=", columns);
    }

    // A REFUSAL IS THE OTHER TENSE OF THE SAME SENTENCE, and carries the reason the
    // layer that detected it wrote.
    ui.sample.ok = false;
    ui.sample.lines.clear();
    ui.sample.reason = "'x' is an operator and not a source: it declares 1 input (a)";
    const std::vector<std::string> refused = powers_text(ui, 20, 70);
    CHECK(row_with_text(refused, intro::kSampleRefusedWord) >= 0);
    CHECK(row_with_text(refused, "is an operator and not a source") >= 0);
}

TEST_CASE("SOURCE-1: an unshowable sample line is COUNTED rather than dropped") {
    intro::PowersUi ui = showing(four_cells());
    ui.select("math.max");
    ui.sample.present = true;
    ui.sample.identity = "math.max";
    ui.sample.ok = true;
    ui.sample.lines = {"one", "two", "three", "four", "five"};

    bool marked_somewhere = false;
    for (std::int64_t rows = 4; rows <= 20; ++rows) {
        const std::vector<std::string> shown = powers_text(ui, rows, 60);
        const std::int64_t at = row_with_text(shown, intro::kSampledWhenAsked);
        if (at < 0) {
            continue;
        }
        std::size_t seen = 0;
        for (const char* line : {"one", "two", "three", "four", "five"}) {
            seen += row_with_text(shown, std::string("  ") + line) >= 0 ? 1u : 0u;
        }
        const bool counted = row_with_text(shown, "more") >= 0;
        if (seen < 5) {
            CHECK_MESSAGE(counted, "rows=", rows); // nothing is hidden in silence
            marked_somewhere = true;
        }
    }
    CHECK(marked_somewhere);
}

TEST_CASE("SOURCE-1: a sample survives filters, view switches and a lost population") {
    intro::PowersUi ui = showing(four_cells());
    ui.sample.present = true;
    ui.sample.identity = "zengine.recipes.catalog";
    ui.sample.ok = true;
    ui.sample.lines = {"zengine.RecipeCatalog v1"};

    // FILTERED OUT OF THE LIST: still shown, because a filter is not evidence about
    // what a Source said.
    ui.query.type("logic");
    ui.view = intro::powers_view::kOperators;
    CHECK(row_with_text(powers_text(ui, 20, 70), "zengine.RecipeCatalog v1") >= 0);

    // THE PROVIDER UNLOADED: still shown. The answer was true when it was given, and a
    // later reading of the catalog is a fact about the catalog rather than about it.
    ui.query.clear();
    ui.view = intro::powers_view::kSources;
    ui.reading = ws::ResolvedPowers{};
    intro::revalidate(ui);
    CHECK(ui.selected().empty());
    const std::vector<std::string> after = powers_text(ui, 20, 70);
    CHECK(row_with_text(after, intro::kSampledWhenAsked) >= 0);
    CHECK(row_with_text(after, "zengine.RecipeCatalog v1") >= 0);
    CHECK(row_with_text(after, intro::kNoSourcesHere) >= 0); // and the list is honest too
}

TEST_CASE("SOURCE-1: the detail block names what a sample would yield, without sampling") {
    intro::PowersUi ui = showing(four_cells());
    ui.select("zengine.recipes.catalog");
    const std::vector<std::string> shown = powers_text(ui, 20, 70);
    CHECK(row_with_text(shown, "yields zengine.RecipeCatalog v1") >= 0);
    CHECK(row_with_text(shown, "active    zengine.workshop.host") >= 0);
    CHECK(row_with_text(shown, intro::kSampleControl) >= 0);

    // ⭐ AND IT IS A READ OF WHAT REGISTRATION ALREADY CARRIED. The schema identity is
    // in the reading; producing it needed no evaluator, no describe across a seam, and
    // no guess from the identity's spelling.
    ui.select("math.max");
    CHECK(row_with_text(powers_text(ui, 20, 70), "yields zengine.MaxSoFar v1") >= 0);

    // A CONTRIBUTION THE HOST PUBLISHED ITSELF IS NAMED AS THE HOST, and an empty
    // provider never reaches a maker's eye as an empty column.
    ws::ResolvedPowers hosted;
    hosted.powers.push_back(power_of("host.thing", {source_from("")}));
    intro::PowersUi own = showing(hosted);
    own.select("host.thing");
    CHECK(row_with_text(powers_text(own, 20, 70), intro::kHostItself) >= 0);

    // A STACK IS SHOWN ACTIVE-FIRST, which is the wire's order reversed deliberately:
    // a maker reads top-down and the first thing they should read is the one whose
    // code runs.
    ws::ResolvedPowers stacked;
    stacked.powers.push_back(power_of("covered", {source_from("under"), source_from("over")}));
    intro::PowersUi deep = showing(stacked);
    deep.select("covered");
    const std::vector<std::string> rows = powers_text(deep, 20, 70);
    const std::int64_t live = row_with_text(rows, "active    over");
    const std::int64_t buried = row_with_text(rows, "shadowed  under");
    REQUIRE(live >= 0);
    REQUIRE(buried >= 0);
    CHECK(live < buried);
}

TEST_CASE("SOURCE-1: only a selected Source offers a sample gesture") {
    intro::PowersUi ui = showing(four_cells());
    CHECK(intro::sampleable(ui).empty()); // nothing selected

    ui.view = intro::powers_view::kSources;
    ui.select("math.max");
    CHECK(intro::sampleable(ui) == "math.max");

    // ⭐ THE OPERATORS VIEW OFFERS NONE, AND NOT BY REFUSING ONE: there is no control,
    // no identity to spend, and nothing for an operator-invocation surface to grow
    // from. `op::sample` owns the refusal at the spend; this owns the absence.
    ui.view = intro::powers_view::kOperators;
    ui.select("logic.select_int");
    CHECK(intro::sampleable(ui).empty());
    const intro::PowersView view = intro::project_powers_ui(ui, 20, 70);
    CHECK(place_of(view, intro::powers_control::kSample).first == -1);
    CHECK(row_with_text(texts_of(view.rows), intro::kSampleControl) == -1);

    // AND A SELECTION THE CURRENT READING NO LONGER HAS OFFERS NONE EITHER.
    intro::PowersUi stale = showing(four_cells());
    stale.select("math.max");
    stale.reading = ws::ResolvedPowers{};
    CHECK(intro::sampleable(stale).empty());
}

TEST_CASE("SOURCE-1: the census is slack-only and keeps QR-4's grammar") {
    // THE CATALOG CENSUS AND THE POSITION MARKER ARE TWO DIFFERENT COUNTS. `17/143`
    // counts the list being navigated; this counts the whole reading, and it waits for
    // room nothing else wanted.
    ws::ResolvedPowers one;
    one.providers = {"zengine.operators.basic"};
    one.powers.push_back(power_of("math.max", {source_from("zengine.operators.basic")}));
    CHECK(row_with_text(powers_text(showing(one), 20, 70),
                        "1 power resolves here -- from 1 provider") >= 0);

    ws::ResolvedPowers two_from_one;
    two_from_one.providers = {"zengine.operators.basic"};
    two_from_one.powers.push_back(power_of("math.max", {source_from("zengine.operators.basic")}));
    two_from_one.powers.push_back(
        power_of("logic.select_int", {source_from("zengine.operators.basic")}));
    const std::vector<std::string> pair = powers_text(showing(two_from_one), 20, 70);
    CHECK(row_with_text(pair, "2 powers resolve here -- from 1 provider") >= 0);
    CHECK(row_with_text(pair, "1 providers") == -1);

    CHECK(row_with_text(powers_text(showing(ws::ResolvedPowers{}), 20, 70),
                        "0 powers resolve here -- from 0 providers") >= 0);

    // ⭐ AND THE SENTENCE THAT BOUNDS EVERY COUNT IS STILL THERE, unchanged, saying
    // only what this pane actually read.
    CHECK(std::string(intro::kHostResolution) ==
          "this pane describes this host's operator resolution only");
    const std::vector<std::string> shown = powers_text(showing(four_cells()), 20, 70);
    REQUIRE(row_with_text(shown, intro::kHostResolution) >= 0);
    for (const char* claim : {"took no offer", "its own catalog", "holds its own"}) {
        CHECK_MESSAGE(row_with_text(shown, claim) == -1, claim);
    }

    // THE LIST OUTRANKS THE CENSUS. A pane that had to window its own list spends that
    // row on the list instead.
    const std::vector<std::string> cramped = powers_text(showing(many_sources(40)), 8, 60);
    CHECK(row_with_text(cramped, "resolve here") == -1);
    CHECK(row_with_text(cramped, "more below") >= 0);
}

TEST_CASE("INTR-1: an entry and its omission marker are ONE demand on the budget") {
    // THE ARITHMETIC INTR-0 WAS MEASURED GETTING WRONG, generalised to blocks and
    // spelled once. Three blocks of two rows in a budget of five: two blocks fit, and
    // the marker's row takes one of them back rather than being added on top.
    const std::vector<std::int64_t> heights{2, 2, 2};
    CHECK(intro::lay_blocks(heights, 6).shown == 3);
    CHECK_FALSE(intro::lay_blocks(heights, 6).marker);
    CHECK(intro::lay_blocks(heights, 5).shown == 2);
    CHECK(intro::lay_blocks(heights, 5).marker);
    CHECK(intro::lay_blocks(heights, 4).shown == 1);
    CHECK(intro::lay_blocks(heights, 4).marker);
    // AT A BUDGET TOO SMALL FOR ONE BLOCK, IT SAYS. Nothing is hidden in silence.
    CHECK(intro::lay_blocks(heights, 1).shown == 0);
    CHECK(intro::lay_blocks(heights, 1).marker);
    // TOTAL over every budget, including ones no pane has.
    CHECK(intro::lay_blocks(heights, 0).shown == 0);
    CHECK_FALSE(intro::lay_blocks(heights, 0).marker);
    CHECK(intro::lay_blocks({}, 5).shown == 0);
    CHECK_FALSE(intro::lay_blocks({}, 5).marker);
}

// ---- Tier one, the sample presenter: an admitted Value, as prose (SOURCE-1) --------
//
// `render_value` is the only maker-facing spelling of a sampled value in this
// repository, and it is host-side because it HAS to be: the pane's image is a woven
// weave whose accept-set is closed and links no operator code, so a value claiming a
// schema nobody compiled against cannot cross to it. What crosses is these lines.
//
// EVERY CASE HERE IS OVER A VALUE. The function is pure -- no clock, no counter, no
// static, no identity of the Source that produced it -- so what it MEANS is provable
// without a catalog, and the two cases that DO use the real host Sources use them to
// prove the generic path renders them, not to teach it about them.

TEST_CASE("SOURCE-1: both real host Sources render through one identity-agnostic path") {
    // ⭐ THE TWO SHIPPED SOURCES, THROUGH THE REAL SAMPLE SEAM. `mount_host_sources`
    // installs them, `op::sample` spends them, and the renderer sees only a Value --
    // there is no identity, no schema name and no special case anywhere in it.
    std::string anchor = "D:/projects/my-thing";
    CurrentRecipes recipes;
    op::Catalog catalog;
    const op::MountReport mounted =
        mount_host_sources(catalog, host_sources(anchor, recipes));
    REQUIRE_MESSAGE(mounted.ok, mounted.reason);

    const op::Evaluation project = op::sample(catalog, kProjectAnchorSource);
    REQUIRE_MESSAGE(project.ok(), project.reason());
    const std::vector<std::string> anchored = render_value(project.value());
    REQUIRE(anchored.size() == 2);
    CHECK(anchored[0] == "zengine.ProjectAnchor v1");
    CHECK(anchored[1] == "  anchor  \"D:/projects/my-thing\"");

    const op::Evaluation catalogued = op::sample(catalog, kRecipeCatalogSource);
    REQUIRE_MESSAGE(catalogued.ok(), catalogued.reason());
    const std::vector<std::string> lines = render_value(catalogued.value());
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "zengine.RecipeCatalog v1");
    CHECK(lines[1] == "  catalog");
    CHECK(lines[2] == "    source   \"\"");
    CHECK(lines[3] == "    recipes  0");

    // ⭐ AND THE INTEGER IS AN INTEGER. `loom::compat::serialize` renders it as a JSON
    // *string* -- which is the measurement that disqualified the one total codec this
    // repository already had: a maker reading `"0"` cannot tell a count from a caption.
    const std::string debug = loom::compat::serialize(catalogued.value());
    CHECK(debug.find("\"recipes\":\"0\"") != std::string::npos);
    CHECK(lines[3].find('"') == std::string::npos);
}

TEST_CASE("SOURCE-1: the presenter is total over every kind, and marks what it cannot spell") {
    const std::shared_ptr<const loom::Schema> inner =
        loom::SchemaBuilder("zengine.Inner", 2).field("deep", loom::Kind::Int).build();
    const std::shared_ptr<const loom::Schema> shape =
        loom::SchemaBuilder("zengine.Every", 3)
            .field("count", loom::Kind::Int)
            .field("ratio", loom::Kind::Float)
            .field("label", loom::Kind::Text)
            .field("flag", loom::Kind::Bool)
            .field("blob", loom::Kind::Bytes)
            .message("nested", inner)
            .field("missing", loom::Kind::Text, /*required=*/false)
            .build();

    loom::Value nested(inner);
    nested.set("deep", loom::Cell::integer(-7));
    loom::Value v(shape);
    v.set("count", loom::Cell::integer(42));
    v.set("ratio", loom::Cell::real(1.5));
    v.set("label", loom::Cell::text("hello"));
    v.set("flag", loom::Cell::boolean(true));
    v.set("blob", loom::Cell::bytes(loom::Bytes{1, 2, 3, 4}));
    v.set("nested", loom::Cell::message(std::move(nested)));

    const std::vector<std::string> lines = render_value(v);
    REQUIRE(lines.size() == 9);
    CHECK(lines[0] == "zengine.Every v3");
    CHECK(row_with_text(lines, "count    42") >= 0);   // digits, never a quoted string
    CHECK(row_with_text(lines, "ratio    1.5") >= 0);  // and not `1.500000`
    CHECK(row_with_text(lines, "label    \"hello\"") >= 0); // quotes say Text
    CHECK(row_with_text(lines, "flag     true") >= 0);
    CHECK(row_with_text(lines, "blob     (bytes, 4 octets)") >= 0); // honest, not base64
    CHECK(row_with_text(lines, "  nested") >= 0);
    CHECK(row_with_text(lines, "    deep  -7") >= 0);  // the nested schema's own column
    CHECK(row_with_text(lines, "missing  (absent)") >= 0); // an observed absence

    // AND IT IS PURE: the same value renders identically, always.
    CHECK(render_value(v) == lines);
}

TEST_CASE("SOURCE-1: Text is quoted and made safe for the row it will become") {
    const std::shared_ptr<const loom::Schema> shape =
        loom::SchemaBuilder("zengine.Said", 1).field("text", loom::Kind::Text).build();
    loom::Value v(shape);
    v.set("text", loom::Cell::text("a\nb\t\"c\"\\d\xC3\xAF"));
    const std::vector<std::string> lines = render_value(v);
    REQUIRE(lines.size() == 2);

    // ⭐ THESE LINES BECOME `SurfaceTextRow`s, whose contract is plain printable ASCII
    // -- one canvas cell per BYTE -- and Workshop refuses a whole update for one byte
    // it cannot draw. So every byte is still REPORTED, in the one notation a reader
    // already knows, rather than filtered away or left to poison the pane.
    for (const std::string& line : lines) {
        for (const char c : line) {
            const unsigned char byte = static_cast<unsigned char>(c);
            CHECK(byte >= 0x20u);
            CHECK(byte < 0x7Fu);
        }
    }
    CHECK(lines[1].find("\\x0A") != std::string::npos);
    CHECK(lines[1].find("\\x09") != std::string::npos);
    CHECK(lines[1].find("\\\"c\\\"") != std::string::npos);
    CHECK(lines[1].find("\\\\d") != std::string::npos);
    CHECK(lines[1].find("\\xC3\\xAF") != std::string::npos);
}

TEST_CASE("SOURCE-1: a list is bounded and its remainder is COUNTED") {
    const std::shared_ptr<const loom::Schema> shape =
        loom::SchemaBuilder("zengine.Many", 1)
            .list("names", loom::type_of(loom::Kind::Text))
            .build();
    loom::Cell::Array members;
    for (int i = 0; i < 20; ++i) {
        members.push_back(loom::Cell::text("n" + std::to_string(i)));
    }
    loom::Value v(shape);
    v.set("names", loom::Cell::list(std::move(members)));

    const std::vector<std::string> lines = render_value(v);
    CHECK(row_with_text(lines, "names  20 items") >= 0); // the TOTAL, before any is cut
    std::size_t spelled = 0;
    for (const std::string& line : lines) {
        spelled += line.find("[") != std::string::npos ? 1u : 0u;
    }
    CHECK(spelled == ws::kSampleMaxListItems);
    CHECK(row_with_text(lines, "... 12 more") >= 0); // and the remainder counted exactly

    // A SHORT LIST IS SHOWN WHOLE AND MARKS NOTHING.
    loom::Value few(shape);
    few.set("names", loom::Cell::list(loom::Cell::Array{loom::Cell::text("only")}));
    const std::vector<std::string> small = render_value(few);
    CHECK(row_with_text(small, "names  1 item") >= 0); // the noun agrees with the number
    CHECK(row_with_text(small, "[0]  \"only\"") >= 0);
    CHECK(row_with_text(small, "more") == -1);

    // AN EMPTY LIST IS AN OBSERVED ZERO.
    loom::Value none(shape);
    none.set("names", loom::Cell::list(loom::Cell::Array{}));
    CHECK(row_with_text(render_value(none), "names  0 items") >= 0);
}

TEST_CASE("SOURCE-1: depth is bounded and the cut says so") {
    // A CHAIN DEEPER THAN THE BOUND. Nothing is silently missing: the walk stops and
    // the line where it stopped names the LIMIT rather than pretending the value ended.
    std::shared_ptr<const loom::Schema> level =
        loom::SchemaBuilder("zengine.Leaf", 1).field("value", loom::Kind::Int).build();
    std::vector<std::shared_ptr<const loom::Schema>> schemas{level};
    for (int i = 0; i < 8; ++i) {
        level = loom::SchemaBuilder("zengine.Level" + std::to_string(i), 1)
                    .message("under", level)
                    .build();
        schemas.push_back(level);
    }
    loom::Value leaf(schemas[0]);
    leaf.set("value", loom::Cell::integer(1));
    loom::Cell held = loom::Cell::message(std::move(leaf));
    for (std::size_t i = 1; i < schemas.size(); ++i) {
        loom::Value up(schemas[i]);
        up.set("under", std::move(held));
        held = loom::Cell::message(std::move(up));
    }
    const std::shared_ptr<loom::Value>& top = held.as_message();
    REQUIRE(top != nullptr);

    const std::vector<std::string> lines = render_value(*top);
    CHECK(static_cast<std::int64_t>(lines.size()) <= ws::kSampleMaxDepth + 2);
    CHECK(row_with_text(lines, ws::kSampleDeeper) >= 0);
    CHECK(row_with_text(lines, "value") == -1); // the leaf really is not shown
}

TEST_CASE("SOURCE-1: a very wide value is cut at the line backstop, and marked") {
    // THE ONLY CUT WHOSE REMAINDER CANNOT BE COUNTED, so it is the only mark that
    // carries no number -- and it says that in words rather than by stopping quietly.
    loom::SchemaBuilder wide("zengine.Wide", 1);
    for (int i = 0; i < 200; ++i) {
        wide.field("f" + std::to_string(i), loom::Kind::Int);
    }
    const std::shared_ptr<const loom::Schema> shape = wide.build();
    loom::Value v(shape);
    for (int i = 0; i < 200; ++i) {
        v.set("f" + std::to_string(i), loom::Cell::integer(i));
    }
    const std::vector<std::string> lines = render_value(v);
    CHECK(lines.size() == ws::kSampleMaxLines + 1);
    CHECK(lines.back().find(ws::kSampleTooLong) != std::string::npos);
}

TEST_CASE("SOURCE-1: a message with no fields is an answer, not a silence") {
    const std::shared_ptr<const loom::Schema> empty =
        loom::make_schema("zengine.Nothing", 4, std::vector<loom::Field>());
    const std::vector<std::string> lines = render_value(loom::Value(empty));
    REQUIRE(lines.size() == 1);
    // THE SCHEMA IDENTITY IS THE FLOOR OF THE ACCOUNTING: even where a value carries
    // no field, what it CLAIMS to be is still said.
    CHECK(lines[0] == "zengine.Nothing v4");
}

// ---- Tier two: the real library, over a real arrangement and a real catalog --------

TEST_CASE("INTR-1: the Arrangement pane shows what THIS host actually resolved") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kArrangementPane);

    // ---- AT THE DEVELOPER'S DEFAULT PANE SIZE, AND THIS IS THE HONEST HALF -------
    //
    // `kStackRows` is nine, so a pane's body is EIGHT prose rows. An artifact's block
    // is three or four, so a default-sized pane shows the count, one artifact, and how
    // many it could not show. That is a real presentation limitation and it is reported
    // rather than papered over: what a maker must never read is a list that looks
    // complete, and what they read here is `... 2 more`.
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0] == "3 of 3 artifacts resolved -- 1 providers, 2 weaves");
        CHECK(any_row(shown, "zengine-operators-basic"));
        CHECK(any_row(shown, "... 2 more"));
        CHECK(any_row(shown, intro::kNotAuthored));
    }

    // ---- AND A MAKER WHO WANTS THE WHOLE PROJECT MAKES THE PANE TALLER (WIND-2) ---
    //
    // No pane code changed, no projection changed, and nothing was told: a bigger room
    // is a room grant, which is this tool's one beat.
    make_taller(r, intro::kArrangementPane, 22);
    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());
    CHECK(shown[0] == "3 of 3 artifacts resolved -- 1 providers, 2 weaves");
    CHECK(any_row(shown, "zengine-operators-basic"));
    CHECK(any_row(shown, "zengine-plain-weave"));
    CHECK(any_row(shown, intro::kIntrospectionStem));
    CHECK_FALSE(any_row(shown, "more"));
    // ...and the resolved provider identity is the one the ARTIFACT declared about
    // itself, which is nowhere in the plan.
    CHECK(any_row(shown, "zengine.operators.basic"));
    // BOTH KINDS OF FACT, LABELLED, WHERE A MAKER READS THEM.
    CHECK(any_row(shown, "authored  provider normal"));
    CHECK(any_row(shown, "resolved  provider zengine.operators.basic, 2 powers"));
    CHECK(any_row(shown, "authored  weave " + std::string(kIntroOffice)));
    // AND THE FACT IS BOUNDED, AND ITS SOURCE NAMED.
    CHECK(any_row(shown, intro::kNotAuthored));
    CHECK(any_row(shown, "plan: default-load-plan.json"));
}

TEST_CASE("INTR-1: a provider-only artifact is in Arrangement and NOT in Loaded") {
    // ⭐ THE APPARENT DISAGREEMENT, MEASURED. `zengine-operators-basic` is a provider
    // and not a weave: no Kernel loads it, it has no WeaveId and no role. It is a row
    // of one pane and absent from the other, and a build in which both listed it would
    // be a build in which one of them had started guessing.
    PaneRig r;
    const std::int64_t arrangement = open_intro_pane(r, intro::kArrangementPane);
    const std::vector<std::string> project = pane_rows(r, arrangement);
    CHECK(any_row(project, "zengine-operators-basic"));

    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::vector<std::string> loaded = pane_rows(r, intro_row(r, kIntroPane)->kind);
    REQUIRE_FALSE(loaded.empty());
    CHECK_FALSE(any_row(loaded, "zengine-operators-basic"));
    // ...and the Loaded pane is still exactly what it always was: the KERNEL's map.
    CHECK(loaded[0] == "loaded weaves -- 2");
    CHECK(any_row(loaded, std::string(intro::kIntrospectionStem) + " @" + kIntroOffice));
    CHECK(any_row(loaded, intro::kNotInProcess));
    CHECK_FALSE(r.kernel.is_loaded("zengine-operators-basic"));
}

TEST_CASE("INTR-1: THE OVERLAY WITNESS, through the pane a maker actually reads") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);

    // ---- THE OPERATORS VIEW, WHICH IS WHERE THIS HOST'S POWERS ARE (SOURCE-1) ---
    //
    // This arrangement mounts no host Sources, so `Sources` is honestly empty and the
    // maker's first act is to switch. `Tab` does it, through the real key seam: a press
    // on a row that means nothing points the keyboard here, and then one key.
    make_taller(r, intro::kPowersPane, 16);
    focus_pane(r, kind);
    r.key(input::scan::kTab);

    // ---- BASELINE -------------------------------------------------------------
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0].find("[Operators]") != std::string::npos);
        CHECK(shown[0].find("-/2") != std::string::npos); // two operators, none chosen yet
        CHECK(any_row(shown, "math.max"));
        CHECK(any_row(shown, "logic.select_int"));
        CHECK_FALSE(any_row(shown, "shadowed"));
        CHECK(any_row(shown, "2 powers resolve here -- from 1 provider"));
    }

    // A MAKER SELECTS ONE, and the detail says whose contribution satisfies it.
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        const std::int64_t at = row_with_text(shown, "math.max");
        REQUIRE(at >= 0);
        press_pane(r, kind, at, 1);
    }
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        const std::int64_t at = row_with_text(shown, "math.max");
        REQUIRE(at >= 0);
        CHECK(shown[static_cast<std::size_t>(at)].rfind(intro::kSelectedMark, 0) == 0);
        CHECK(any_row(shown, "active    zengine.operators.basic"));
        CHECK_FALSE(any_row(shown, "shadowed"));
    }

    // ---- THE OVERLAY, MOUNTED INTO THE HOST'S OWN CATALOG ----------------------
    //
    // NOBODY IS TOLD. No event exists, nothing polls, and the pane's rows are unmoved
    // until it is re-granted room -- which is a resize, which is this tool's one beat.
    const op::MountResult covered =
        op::mount_provider(r.catalog, PROVIDER_MIN_SO, op::MountMode::Overlay);
    REQUIRE_MESSAGE(covered.ok, covered.reason);
    CHECK_FALSE(any_row(pane_rows(r, kind), "shadowed"));

    r.extent(150, 44);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        // THE VIEW, THE SELECTION AND THE CURSOR ALL SURVIVED THE FRESH READING,
        // because the identity is still in the population it names.
        CHECK(shown[0].find("[Operators]") != std::string::npos);
        const std::int64_t at = row_with_text(shown, "math.max");
        REQUIRE(at >= 0);
        CHECK(shown[static_cast<std::size_t>(at)].rfind(intro::kSelectedMark, 0) == 0);
        // ...AND THE STACK UNDER IT MOVED, active first.
        const std::int64_t live = row_with_text(shown, "active    zengine.operators.test.min");
        const std::int64_t buried = row_with_text(shown, "shadowed  zengine.operators.basic");
        REQUIRE(live >= 0);
        REQUIRE(buried >= 0);
        CHECK(live < buried);
        CHECK(any_row(shown, "2 powers resolve here -- from 2 providers"));
    }

    // ---- UNMOUNTED: THE ONE UNDERNEATH IS REVEALED ----------------------------
    REQUIRE(r.catalog.unmount("zengine.operators.test.min"));
    r.extent(160, 48);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(any_row(shown, "active    zengine.operators.basic"));
        CHECK_FALSE(any_row(shown, "shadowed"));
        CHECK(any_row(shown, "2 powers resolve here -- from 1 provider"));
    }
    // NOTHING IN THE PANE'S SOURCE MOVED BETWEEN THOSE THREE READINGS.
}

TEST_CASE("QR-4: the corrected wording reaches a maker's eye WHOLE, off the real canvas") {
    // THE SAME TWO REPAIRS READ BACK FROM THE PUBLISHED CANVAS, because a projection
    // that is right and a pane that is too narrow to say it are not the same result --
    // `fit` would have marked the difference with `...` and no tier-one case could see
    // it. This host resolves two powers from ONE provider, which is the singular fixture.
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    make_taller(r, intro::kPowersPane, 16);
    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());

    CHECK(any_row(shown, "2 powers resolve here -- from 1 provider"));
    CHECK_FALSE(any_row(shown, "1 providers"));

    const std::int64_t bound = row_with_text(shown, intro::kHostResolution);
    REQUIRE_MESSAGE(bound >= 0, "rows=", shown.size());
    CHECK(shown[static_cast<std::size_t>(bound)] == intro::kHostResolution);
    CHECK_FALSE(any_row(shown, "took no offer"));
    CHECK_FALSE(any_row(shown, "its own catalog"));
}

TEST_CASE("INTR-1: a provider nobody named appears in the pane with no source edit") {
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    make_taller(r, intro::kPowersPane, 16);
    focus_pane(r, kind);
    r.key(input::scan::kTab); // the Operators view: this host's powers all take arguments
    CHECK_FALSE(any_row(pane_rows(r, kind), "prov.function"));

    const op::MountResult added =
        op::mount_provider(r.catalog, PROVIDER_A_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(added.ok, added.reason);
    r.extent(150, 44); // a room grant, which is this tool's one beat

    const std::vector<std::string> shown = pane_rows(r, kind);
    REQUIRE_FALSE(shown.empty());
    CHECK(any_row(shown, "5 powers resolve here -- from 2 providers"));
    for (const char* power : {"prov.function.1", "prov.function.2", "prov.function.3"}) {
        CHECK_MESSAGE(any_row(shown, power), power);
    }
    // ...AND THE COMPOSITES SAY SO, off the definitions and with nothing here
    // classifying anything. This is the feature: a power added later is a row.
    CHECK(any_row(shown, "(composite)"));

    // AND THE COMPOSITE-ONLY FILTER IS POINTER-OPERABLE, over the same population.
    const std::int64_t at = chrome_column(r, kind, "[ ] " + std::string(intro::kCompositeWord));
    REQUIRE(at >= 0);
    press_pane(r, kind, 0, at);
    const std::vector<std::string> filtered = pane_rows(r, kind);
    CHECK(filtered[0].find("[x] " + std::string(intro::kCompositeWord)) != std::string::npos);
    CHECK(any_row(filtered, "(composite)"));
    CHECK_FALSE(any_row(filtered, "math.max"));
    // ...and the identity of the provider that supplied them is unchanged by any of it.
    press_pane(r, kind, 0, at);
    CHECK(any_row(pane_rows(r, kind), "math.max"));
}

TEST_CASE("SOURCE-1: knowing a power is still not authority to change it") {
    // ⭐ INTR-1'S LAW, RE-ASKED OF A PANE THAT NOW HAS CONTROLS. `Powers` gained view
    // switching, selection, search and one explicit sample; it gained NO way to mount,
    // unmount, overlay, replace, reload, disable or activate anything, and pressing
    // every place in it -- controls included -- proves it against the live catalog
    // rather than against the row text.
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    make_taller(r, intro::kPowersPane, 16);
    const std::vector<std::string> providers_before = r.catalog.providers();
    const std::vector<std::string> identities_before = r.catalog.identities();
    const std::size_t panels_before = r.session().panels.open.size();
    const std::uint64_t ran_before = op::invocations();

    for (std::int64_t pass = 0; pass < 2; ++pass) {
        const std::vector<std::string> shown = pane_rows(r, kind);
        for (std::int64_t row = 0; row < static_cast<std::int64_t>(shown.size()); ++row) {
            for (std::int64_t column = 0; column < 40; column += 3) {
                press_pane(r, kind, row, column);
            }
        }
    }
    CHECK(r.catalog.providers() == providers_before);
    CHECK(r.catalog.identities() == identities_before);
    CHECK(r.session().panels.open.size() == panels_before);
    CHECK(r.load_refusals.empty());
    // ⭐ AND NOT ONE EVALUATOR RAN. Every control in this pane except `[ Sample ]` is a
    // presentation decision, and `[ Sample ]` needs a selected SOURCE -- which this
    // arrangement, whose powers all take arguments, does not have.
    CHECK(op::invocations() == ran_before);

    // AND NO ROW OF THE PROJECT PANE OFFERS A CONTROL EITHER -- it is unchanged.
    r.pick(PaneRef{kIntroOffice, intro::kArrangementPane});
    REQUIRE(intro_row(r, intro::kArrangementPane) != nullptr);
    const std::int64_t project = intro_row(r, intro::kArrangementPane)->kind;
    const std::vector<std::string> rows = pane_rows(r, project);
    for (std::int64_t row = 0; row < static_cast<std::int64_t>(rows.size()); ++row) {
        press_pane(r, project, row, 1);
    }
    CHECK(pane_rows(r, project) == rows);
    for (const std::string& row : rows) {
        for (const char* verb :
             {"unmount", "replace", "reload", "disable", "activate", "[x]", "(x)"}) {
            CHECK(row.find(verb) == std::string::npos);
        }
    }
}

TEST_CASE("INTR-1: opening a pane asks ONCE, and a quiet bus asks nothing") {
    // NO POLLING LOOP, MEASURED FROM THE BUS. The two questions are counted across a
    // long quiet, an open, and a resize: a pane that polled would show a rising count
    // with nothing having happened.
    PaneRig r;
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    const loom::WeaveId door = r.mount_arrangement();

    std::vector<std::string> asked;
    std::vector<std::string> answered;
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == "PowersRequested" || e.schema_name == "ArrangementRequested") {
            asked.push_back(e.schema_name);
        }
        if (e.sender == door && !e.schema_name.empty()) {
            answered.push_back(e.schema_name);
        }
    });
    r.ready();
    r.extent(160, 48);
    for (int turn = 0; turn < 40; ++turn) {
        r.bus.drain_until_idle();
    }
    // NOTHING IS OPEN, SO NOTHING IS ASKED. A tool that read on a beat would already
    // have spoken here.
    CHECK(asked.empty());
    CHECK(answered.empty());

    r.pick(PaneRef{kIntroOffice, intro::kPowersPane});
    const std::size_t after_open = asked.size();
    CHECK(after_open >= 1);
    for (int turn = 0; turn < 40; ++turn) {
        r.bus.drain_until_idle();
    }
    // ...AND A LONG QUIET AFTER IT ASKS NOTHING MORE. The one beat is the room grant.
    CHECK(asked.size() == after_open);
    r.bus.remove_observer(tap);
    CHECK(answered.size() == after_open);
}

TEST_CASE("INTR-1: all three panes may be open at once, each answering its own room") {
    PaneRig r;
    r.mount_workshop();
    const load::Executed done = r.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    r.mount_arrangement("p.json");
    r.ready();
    r.extent(240, 60);
    r.pick(intro_ref());
    r.pick(PaneRef{kIntroOffice, intro::kArrangementPane});
    r.pick(PaneRef{kIntroOffice, intro::kPowersPane});

    // THREE ROOMS, THREE QUESTIONS, THREE ANSWERS -- and each projected against ITS
    // OWN budget. One shared room would have made the last grant decide how the other
    // two were drawn.
    const std::vector<std::string> loaded = pane_rows(r, intro_row(r, kIntroPane)->kind);
    const std::vector<std::string> project =
        pane_rows(r, intro_row(r, intro::kArrangementPane)->kind);
    const std::vector<std::string> powers =
        pane_rows(r, intro_row(r, intro::kPowersPane)->kind);
    REQUIRE_FALSE(loaded.empty());
    REQUIRE_FALSE(project.empty());
    REQUIRE_FALSE(powers.empty());
    CHECK(loaded[0].rfind("loaded weaves -- ", 0) == 0);
    CHECK(project[0].find("artifacts resolved") != std::string::npos);
    // ...and the third one's first row is its chrome, which is the pane saying which
    // of its two questions it is currently answering (SOURCE-1).
    CHECK(powers[0].find(intro::kSourcesWord) != std::string::npos);
    CHECK(powers[0].find(intro::kOperatorsWord) != std::string::npos);
    // ...and each stayed inside the room it was granted.
    for (const char* pane : {kIntroPane, intro::kArrangementPane, intro::kPowersPane}) {
        const std::int64_t kind = intro_row(r, pane)->kind;
        const ExternalPane* room = r.session().panels.external_pane(kind);
        REQUIRE(room != nullptr);
        CHECK(static_cast<std::int64_t>(pane_rows(r, kind).size()) <= room->rows);
    }
}

TEST_CASE("INTR-1: the graphical medium grants a different room and both panes spend it") {
    // BOTH MEDIA, ONE PANE SEMANTIC. The provider is handed `rows` and `columns` and
    // never a cell, a pixel, a font or the identity of the medium that answered -- so
    // what differs between a terminal reading and a graphical one is a pair of integers
    // `fit_region` resolved on Workshop's side, and nothing else.
    //
    // The metric arrives as a NUMBER, which is how every medium-dependent claim in this
    // suite since HD-6 has been proved on a lane with no font engine.
    PaneRig r;
    const std::int64_t kind = open_intro_pane(r, intro::kPowersPane);
    const ExternalPane* cells = r.session().panels.external_pane(kind);
    REQUIRE(cells != nullptr);
    const std::int64_t cell_cols = cells->columns;
    const std::vector<std::string> in_cells = pane_rows(r, kind);
    REQUIRE_FALSE(in_cells.empty());

    // A REAL FACE'S METRIC over the same surface: a 10-pixel advance is more columns in
    // the same rectangle, and an 18-pixel line in a 12-pixel cell is fewer prose rows.
    r.extent(1200, 500, 10, 18);
    const ExternalPane* graphical = r.session().panels.external_pane(kind);
    REQUIRE(graphical != nullptr);
    CHECK(graphical->columns != cell_cols);

    const std::vector<std::string> in_pixels = pane_rows(r, kind);
    REQUIRE_FALSE(in_pixels.empty());
    // THE SAME TRUTH IN BOTH, which is the honesty claim: two projections of one fact.
    // The chrome row is composed to the width it was GIVEN, so the two are not
    // byte-identical -- what is identical is what they say: which view, and where in it.
    CHECK(in_cells[0].find("[Sources]") != std::string::npos);
    CHECK(in_pixels[0].find("[Sources]") != std::string::npos);
    CHECK(in_cells[0].find("-/0") != std::string::npos);
    CHECK(in_pixels[0].find("-/0") != std::string::npos);
    // ...AND THIS ARRANGEMENT HONESTLY HAS NO SOURCES, in both media, said in words
    // rather than left as an empty strip.
    CHECK(any_row(in_cells, intro::kNoSourcesHere));
    CHECK(any_row(in_pixels, intro::kNoSourcesHere));
    // ...AND THE VIEW ANSWERED THE NEW ROOM rather than the old one. Workshop clears its
    // cache before every grant, so a projection that had not moved would read as
    // `waiting` here instead.
    CHECK(static_cast<std::int64_t>(in_pixels.size()) <= graphical->rows);
    for (const std::string& row : in_pixels) {
        CHECK(static_cast<std::int64_t>(row.size()) <= graphical->columns);
    }
}

TEST_CASE("INTR-1: a host with no arrangement door leaves the two panes WAITING") {
    // THE TOOL IS LOADABLE INTO ANY LOOM HOST, and only one in this repository answers
    // for its own project. A host that mounts no door holds no `zengine.arrangement`
    // office, the ask reaches nobody, and the pane says the honest thing -- never
    // `unavailable`, which is a fate nothing here has observed.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    r.ready();
    r.extent(160, 48); // room for two stacked panes, so both are really on the canvas
    r.pick(PaneRef{kIntroOffice, intro::kArrangementPane});
    const std::vector<std::string> shown =
        pane_rows(r, intro_row(r, intro::kArrangementPane)->kind);
    REQUIRE(shown.size() == 1);
    CHECK(shown[0] == std::string(kExternalWaiting));
    // ...and the Loaded pane, whose owner is the Kernel and is always there, is fine.
    r.pick(intro_ref());
    REQUIRE(intro_row(r, kIntroPane) != nullptr);
    const std::vector<std::string> loaded = pane_rows(r, intro_row(r, kIntroPane)->kind);
    REQUIRE_FALSE(loaded.empty());
    CHECK(loaded[0] == "loaded weaves -- 1");
}

// ---- Tier three: Workshop, read as a source file -----------------------------------

TEST_CASE("INTR-1: Workshop knows no pane, and the two new ones are no exception") {
    // DEFENCE IN DEPTH, AND SAID TO BE. Every rig above drives the real seam; what a
    // source read adds is that "Workshop discovers panes rather than being taught
    // them" cannot quietly stop being true while every rig stays green.
    //
    // THE FORBIDDEN FORMS ARE QUOTED LITERALS AND IDENTIFIERS, never bare words. Prose
    // about a pane is not a branch on one, and a tripwire that could not tell the
    // difference would be a tripwire that forbids explaining the code.
    for (const char* path : {WORKSHOP_HOST_CPP, WORKSHOP_WEAVE_HPP, WORKSHOP_SCREEN_HPP,
                             WORKSHOP_PANEL_HPP}) {
        const std::string source = file_source(path);
        for (const char* forbidden : {"\"arrangement\"", "\"powers\"", "\"loaded\"",
                                      "kArrangementPane", "kPowersPane", "kLoadedPane",
                                      "introspection/", "kIntrospectionRole"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
    // ...AND NO `panel::k*` WAS MINTED FOR ANY OF THEM. Every handle these panes carry
    // is a runtime one, minted from a live offer.
    PaneRig r;
    r.mount_workshop();
    (void)r.load(intro::kIntrospectionStem, WORKSHOP_SO_INTROSPECTION, kIntroOffice);
    REQUIRE(r.session().panels.runtime.entries.size() == kIntroPaneCount);
    for (const RuntimePane& row : r.session().panels.runtime.entries) {
        CHECK(is_runtime_kind(row.kind));
    }
}

TEST_CASE("BLD-2: the presentation holds no realization or build-runner reach") {
    // THE FRONTIER MADE THE PRESENTATION AWARE OF REALIZATION, and this is the wire
    // that keeps "aware" from quietly becoming "in charge". What the weave holds is
    // one host-wired READING (`HostContext::frontier`); the one route from a maker's
    // gesture to a realized artifact is still `BuildRequested` to the Builder office,
    // and every rig above stays green if a shortcut arrives -- only a source read can
    // say the shortcut is not there.
    //
    // THE FORBIDDEN FORMS ARE IDENTIFIERS, never bare words: the owner's type, its
    // file, the offer only the TOOL may make, the runner's own order, and the runner's
    // office. A presentation that could spell any of them is a presentation one edit
    // away from building or realizing on its own authority.
    for (const char* path : {WORKSHOP_WEAVE_HPP, WORKSHOP_SCREEN_HPP, WORKSHOP_PANEL_HPP}) {
        const std::string source = file_source(path);
        for (const char* forbidden : {"PlanExecutor", "load_execute", "OfferArtifact",
                                      "RunBuild", "kBuildRunnerRole"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
}

TEST_CASE("INTR-1: the host mounts a door and injects no host-owned object into an artifact") {
    const std::string host = file_source(WORKSHOP_HOST_CPP);

    // THE DOOR IS MOUNTED BEFORE THE PLAN IS PERFORMED, because the tool that asks is
    // an artifact THIS PLAN LOADS: a door mounted afterwards would be absent during the
    // window in which a pane might first be granted room.
    const std::size_t door = host.find("mount_in_office<ArrangementDoor>");
    const std::size_t begin = host.find("executor.begin(read_plan.plan)");
    REQUIRE(door != std::string::npos);
    REQUIRE(begin != std::string::npos);
    CHECK(door < begin);
    // ...AND THE HOST NO LONGER BLOCKS UNTIL THE PROJECT IS REALIZED (BOOT-0). The
    // line that used to be here returned only once every row had settled.
    CHECK(host.find("executor.run(") == std::string::npos);

    // ...AND ITS GRANT IS THE TWO ANSWERS AND NOTHING ELSE.
    CHECK(host.find("say_resolved.allow_to_any(ResolvedArrangement::zen_name") !=
          std::string::npos);
    CHECK(host.find("say_resolved.allow_to_any(ResolvedPowers::zen_name") != std::string::npos);
    CHECK(host.find("say_resolved.allow(") == std::string::npos);

    // NOTHING HOST-OWNED CROSSES INTO A LOADED ARTIFACT. The host's operator surface is
    // handed to the offer machinery and nowhere else; there is no second injected
    // capability, no `ZenHostApi` widening and no service locator.
    for (const char* forbidden : {"ZengineEverythingHostApi", "service_locator",
                                  "IntrospectionRegistry", "ProjectMirror", "OperatorMirror"}) {
        CHECK_MESSAGE(host.find(forbidden) == std::string::npos, forbidden);
    }
}

TEST_CASE("INTR-1: neither projection names a power, a provider or an artifact") {
    // ⭐ THE GENERICITY CLAIM, READ OFF THE SOURCE. A test may name `math.max` because
    // it is verifying known production state; the projection may not, because a
    // provider added later must appear without an edit. That is the feature.
    //
    // QUOTED LITERALS AND IDENTIFIERS, for the tripwire above's reason exactly: these
    // files EXPLAIN what they refuse to branch on, and a check that could not tell a
    // sentence from a special case would forbid the explanation.
    for (const char* path : {INTROSPECTION_RESOLVED_HPP, WORKSHOP_ARRANGEMENT_HPP}) {
        const std::string source = file_source(path);
        for (const char* forbidden : {"\"math.max\"", "\"logic.select_int\"",
                                      "\"timer.normalize_delay\"", "\"zengine.operators",
                                      "\"zengine.timer\"", "\"zengine-timer\"",
                                      "\"zengine-operators-basic\"", "kMaxInt", "kSelectInt",
                                      "kNormalizeDelay", "timer/normalize.hpp",
                                      "operator/primitives.hpp"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, path, " names '",
                          forbidden, "'");
        }
    }
}
