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
// THIS FILE OWNS: the live seam -- what browsing and sampling cost.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

// ============================================================================
// SOURCE-1 — the live seam: browsing runs nothing, sampling runs exactly one
// ============================================================================
//
// Everything below drives the REAL `zengine-introspection` library, loaded through the
// real Kernel and Manager, against a real `op::Catalog` holding the host's own two
// Sources and a real cross-image provider -- because the claims are about a call graph
// and a message path, and a rig that could reach the evaluator by hand would prove
// nothing about either.
//
// THE TWO INSTRUMENTS, AND WHY BOTH ARE NEEDED:
//
//     op::invocations()      the HOST image's native-body counter. It sees the two
//                            host Sources and every operator this executable compiled.
//     prov.source.spends     a Source in ANOTHER image, whose body counts its own
//                            spends and answers 1, then 2, then 3. `op::invocations()`
//                            is a vague-linkage static and is BLIND to it, so this is
//                            the only instrument that can tell an evaluation from a
//                            cached answer across the provider ABI -- and it is the one
//                            that makes "no memoization" a measurement instead of a
//                            promise, because a memo would answer 1 twice.

namespace {

/// The office a second tool would ask the sample door from -- what makes "an office
/// may ask; anonymous speech may not" a claim a case can put either way.
constexpr const char* kAskerOffice = "zengine.test.sampler";

struct SampleBook {
    std::string identity;
    bool anonymously = false; ///< speak personally instead of as the office
    std::vector<SourceSampled> heard;
    std::int64_t refusals = 0;
};

class SampleAsker : public loom::WeaveBase<SampleAsker, SeenState,
                                           loom::Accept<SeatDo, SourceSampled, loom::Refused>,
                                           loom::Emit<SampleRequested>> {
public:
    explicit SampleAsker(SampleBook& book) : book_(&book) {}

    void on(const SeatDo&, loom::Mail& mail) {
        const SampleRequested ask{book_->identity};
        if (book_->anonymously) {
            (void)mail.send_to_role(kSampleRole, ask, ++asked_);
            return;
        }
        (void)mail.as_role(kAskerOffice).send_to_role(kSampleRole, ask, ++asked_);
    }
    void on(const SourceSampled& said, loom::Mail&) { book_->heard.push_back(said); }
    void on(const loom::Refused&, loom::Mail&) { ++book_->refusals; }

private:
    SampleBook* book_;
    std::uint64_t asked_ = 0;
};

/// Ask the sample door directly, once, for one identity.
loom::WeaveId seat_asker(PaneRig& r, SampleBook& book) {
    auto weave = std::make_unique<SampleAsker>(book);
    SampleAsker* raw = weave.get();
    loom::Grant may;
    may.allow_to_role(SampleRequested::zen_name, SampleRequested::zen_version, kSampleRole);
    const loom::WeaveId id =
        r.bus.register_weave(std::move(weave), std::move(may), std::string(kAskerOffice));
    raw->zen_set_self(id);
    return id;
}

void ask_for(PaneRig& r, loom::WeaveId asker, SampleBook& book, std::string identity) {
    book.identity = std::move(identity);
    (void)r.bus.send(asker,
                     loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
    r.bus.drain_until_idle();
}

/// WHAT `prov.source.spends` ANSWERS -- and asking is what makes it move, so a caller
/// has to mean it. The number is process-cumulative (the body's counter lives in the
/// provider's image and survives an unmount), so every case below compares DELTAS.
std::int64_t spend_count(op::Catalog& catalog) {
    const op::Evaluation done = op::sample(catalog, "prov.source.spends");
    REQUIRE_MESSAGE(done.ok(), done.reason());
    const loom::Cell* count = done.value().at(0);
    REQUIRE(count != nullptr);
    return count->as_int();
}

/// The count a rendered sample row on the pane is showing, or -1.
std::int64_t shown_count(const std::vector<std::string>& rows) {
    for (const std::string& row : rows) {
        const std::size_t at = row.find("count  ");
        if (at != std::string::npos) {
            return std::stoll(row.substr(at + 7));
        }
    }
    return -1;
}

/// The row of the Powers pane naming an identity, or -1.
std::int64_t powers_row(PaneRig& r, std::int64_t kind, const std::string& identity) {
    return row_with_text(pane_rows(r, kind), identity);
}

/// Select a power the way a maker does: press the row that names it.
void select_power(PaneRig& r, std::int64_t kind, const std::string& identity) {
    const std::int64_t at = powers_row(r, kind, identity);
    REQUIRE_MESSAGE(at >= 0, identity);
    press_pane(r, kind, at, 1);
}

} // namespace

TEST_CASE("SOURCE-1: browsing the catalog runs no evaluator at all") {
    // ⭐ THE TRIPWIRE THE WHOLE PHASE RESTS ON. Every browsing act a maker has -- a
    // fresh reading, view switching, selection, search, filtering, navigation, the
    // detail block and a repaint -- is performed against a catalog holding two host
    // Sources and a cross-image one, and NOTHING RUNS.
    PaneRig r;
    const op::MountResult sourced =
        op::mount_provider(r.catalog, PROVIDER_SOURCE_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(sourced.ok, sourced.reason);
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);

    // BOTH BASELINES ARE TAKEN AFTER THE FIRST DELIBERATE SAMPLE, because that sample
    // is itself an evaluation and `Catalog::run` counts it in this image whichever
    // image the BODY happens to live in.
    const std::int64_t spent_before = spend_count(r.catalog);
    const std::uint64_t ran_before = op::invocations();
    focus_pane(r, kind);

    // A FRESH READING, twice.
    r.extent(150, 44);
    r.extent(160, 48);
    // THE DERIVATION, both ways.
    r.key(input::scan::kTab);
    r.key(input::scan::kTab);
    // NAVIGATION.
    for (int i = 0; i < 6; ++i) {
        r.key(input::scan::kDown);
    }
    r.key(input::scan::kUp);
    // THE DETAIL BLOCK -- which is where a lazy implementation would have sampled to
    // find out what a Source yields.
    CHECK(any_row(pane_rows(r, kind), "yields"));
    // SEARCH, one character at a time.
    r.text("s");
    r.text("p");
    r.text("e");
    // THE COMPOSITE FILTER.
    const std::int64_t at = chrome_column(r, kind, "[ ] " + std::string(intro::kCompositeWord));
    if (at >= 0) {
        press_pane(r, kind, 0, at);
        press_pane(r, kind, 0, at);
    }
    // AND A REPAINT.
    r.extent(160, 48);

    CHECK(op::invocations() == ran_before);
    // ...AND THE CROSS-IMAGE SOURCE, WHOSE OWN COUNTER THE HOST'S CANNOT SEE, has not
    // run either. Its body counts its own spends, so two deliberate samples that
    // differ by exactly one prove that everything BETWEEN them ran it zero times.
    CHECK(spend_count(r.catalog) == spent_before + 1);
}

TEST_CASE("SOURCE-1: the pane's own image cannot evaluate, and that is structural") {
    // THE OTHER HALF OF THE SAME CLAIM, and the stronger one. `zengine-introspection`
    // links no operator target: there is no `op::Catalog`, no `OperatorDef`, no
    // callable and no `evaluate` in that image to reach, so "browsing does not
    // evaluate" is a fact about the build graph rather than a discipline anybody keeps.
    // READ AS INCLUDES AND LINK LINES, NEVER AS PROSE. Both files EXPLAIN at length
    // what they refuse to reach, so a tripwire on bare identifiers would fire on the
    // paragraph that promises the property -- INTR-1's own tripwire makes exactly this
    // distinction. What is checked here is what the compiler acts on.
    for (const char* file : {INTROSPECTION_CPP, INTROSPECTION_POWERS_HPP}) {
        const std::string source = file_source(file);
        for (const char* forbidden : {"#include \"operator/", "#include <operator/",
                                      "#include \"workshop/host_sources"}) {
            CHECK_MESSAGE(source.find(forbidden) == std::string::npos, file, " ", forbidden);
        }
    }
    // AND THE LINK LINE AGREES: this package names no operator target, so those symbols
    // are not merely unused -- they are not in the image.
    const std::string edges = file_source(INTROSPECTION_CMAKE);
    CHECK(edges.find("zengine-operator") == std::string::npos);
    CHECK(edges.find("zengine-workshop-load") == std::string::npos);
    // ...and the one host header it DOES read is a wire vocabulary and nothing else.
    const std::string weave = file_source(INTROSPECTION_CPP);
    CHECK(weave.find("#include \"workshop/sample_vocabulary.hpp\"") != std::string::npos);
    // AND THE ONE THING THAT DOES LEAVE IS A MESSAGE CARRYING AN IDENTITY.
    CHECK(weave.find("SampleRequested") != std::string::npos);
    const std::shared_ptr<const loom::Schema> ask = loom::schema_of<SampleRequested>();
    REQUIRE(ask != nullptr);
    REQUIRE(ask->fields().size() == 1);
    CHECK(ask->fields()[0].name == "identity");
    CHECK(ask->fields()[0].type.kind == loom::Kind::Text);
}

TEST_CASE("SOURCE-1: an explicit sample is exactly one evaluation, and there is no memo") {
    PaneRig r;
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    focus_pane(r, kind);
    select_power(r, kind, kProjectAnchorSource);

    // ---- RETURN: ONE GESTURE, ONE EVALUATION ---------------------------------
    const std::uint64_t before = op::invocations();
    r.key(input::scan::kReturn);
    CHECK(op::invocations() == before + 1);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(any_row(shown, intro::kSampledWhenAsked));
        CHECK(any_row(shown, "zengine.ProjectAnchor v1"));
        CHECK(any_row(shown, "anchor  \"/zen/pane-rig\""));
    }

    // ---- AGAIN: TWO GESTURES, TWO EVALUATIONS --------------------------------
    r.key(input::scan::kReturn);
    CHECK(op::invocations() == before + 2);

    // ⭐ AND THE OWNER MOVED BETWEEN THEM, WHICH IS WHAT A COUNTER ALONE CANNOT PROVE.
    // A memoised sample would still be showing the launch anchor; this one reads the
    // live owner at the spend, so the answer moves with it.
    r.project_anchor = "/zen/moved-since";
    r.key(input::scan::kReturn);
    CHECK(op::invocations() == before + 3);
    CHECK(any_row(pane_rows(r, kind), "anchor  \"/zen/moved-since\""));

    // ---- AND THE POINTER CONTROL REACHES THE SAME ONE PATH --------------------
    const std::int64_t control = row_with_text(pane_rows(r, kind), intro::kSampleControl);
    REQUIRE(control >= 0);
    r.project_anchor = "/zen/by-hand";
    press_pane(r, kind, control, 3);
    CHECK(op::invocations() == before + 4);
    CHECK(any_row(pane_rows(r, kind), "anchor  \"/zen/by-hand\""));

    // ...AND NOTHING ELSE IN THE PANE DOES. Pressing the row that names the Source
    // selects it and runs nothing, which is what keeps one press one act.
    const std::uint64_t settled = op::invocations();
    select_power(r, kind, kRecipeCatalogSource);
    select_power(r, kind, kProjectAnchorSource);
    CHECK(op::invocations() == settled);
}

TEST_CASE("SOURCE-1: in the Operators view Return invokes nothing") {
    // ⭐ THERE IS NO OPERATOR-INVOCATION SURFACE TO GROW ONE FROM. The gesture is bound
    // to nothing at all: no control is drawn, no identity is spent, and no ask is sent
    // -- so `op::sample`'s own refusal is never even reached, and the pane does not
    // have to hold a second copy of it.
    PaneRig r;
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    focus_pane(r, kind);
    r.key(input::scan::kTab);

    std::vector<std::string> asks;
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == "SampleRequested") {
            asks.push_back(e.schema_name);
        }
    });
    select_power(r, kind, "math.max");
    const std::uint64_t before = op::invocations();
    r.key(input::scan::kReturn);
    r.key(input::scan::kReturn);
    r.bus.remove_observer(tap);

    CHECK(asks.empty());
    CHECK(op::invocations() == before);
    CHECK_FALSE(any_row(pane_rows(r, kind), intro::kSampleControl));
    CHECK_FALSE(any_row(pane_rows(r, kind), intro::kSampledWhenAsked));
}

TEST_CASE("SOURCE-1: the sample crosses one narrow office, and the arrangement door stays narrow") {
    PaneRig r;
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    focus_pane(r, kind);
    select_power(r, kind, kProjectAnchorSource);

    std::vector<std::string> wire;
    const loom::ObserverId tap = r.bus.add_observer([&](const loom::BusEvent& e) {
        if (e.schema_name == "SampleRequested" || e.schema_name == "SourceSampled" ||
            e.schema_name == "PowersRequested" || e.schema_name == "ResolvedPowers") {
            wire.push_back(e.schema_name);
        }
    });
    r.key(input::scan::kReturn);
    r.bus.remove_observer(tap);

    // ⭐ EXACTLY TWO SENTENCES, AND NEITHER IS THE OBSERVATION DOOR'S. A sample does
    // not re-read the catalog projection and the projection does not sample.
    REQUIRE(wire.size() == 2);
    CHECK(wire[0] == "SampleRequested");
    CHECK(wire[1] == "SourceSampled");

    // THE ARRANGEMENT DOOR IS UNCHANGED IN SEMANTIC AUTHORITY: two shapes, and neither
    // of them can cause an evaluation.
    const std::string door = file_source(WORKSHOP_ARRANGEMENT_HPP);
    CHECK(door.find("op::sample") == std::string::npos);
    CHECK(door.find("SampleRequested") == std::string::npos);
    CHECK(door.find("SourceSampled") == std::string::npos);
    CHECK(door.find("overlay, evaluate, load, unload, reload or replace anything") !=
          std::string::npos);

    // AND THE SAMPLE DOOR RETAINS NO PROVIDER, DEFINITION, CALLABLE OR ANSWER.
    const std::string sampler = file_source(WORKSHOP_SAMPLE_DOOR_HPP);
    for (const char* forbidden : {"std::map<", "std::unordered_map<", "std::vector<", "cache_",
                                  "last_", "memo_", "static ", "mutable "}) {
        CHECK_MESSAGE(sampler.find(forbidden) == std::string::npos, forbidden);
    }
    // ITS ONE MEMBER IS A POINTER TO SOMEBODY ELSE'S CATALOG, and there is no second.
    CHECK(sampler.find("const op::Catalog* catalog_;") != std::string::npos);
}

TEST_CASE("SOURCE-1: an office may ask the sample door; anonymous speech may not") {
    PaneRig r;
    const std::int64_t kind = open_powers(r);
    (void)kind;
    SampleBook book;
    const loom::WeaveId asker = seat_asker(r, book);

    // AN OFFICE ASKS AND IS ANSWERED. The rule names nobody -- there is no allow-list
    // and no `zengine.introspection` anywhere in the door -- so a tool added tomorrow
    // asks with no edit there.
    const std::uint64_t before = op::invocations();
    ask_for(r, asker, book, kProjectAnchorSource);
    REQUIRE(book.heard.size() == 1);
    CHECK(book.heard[0].ok);
    CHECK(book.heard[0].identity == kProjectAnchorSource);
    CHECK(op::invocations() == before + 1);

    // ⭐ PERSONAL SPEECH CAUSES NOTHING. Not because the door knows who this is, but
    // because it will not run a body for a sentence with nobody to be answerable to.
    book.anonymously = true;
    ask_for(r, asker, book, kProjectAnchorSource);
    CHECK(book.heard.size() == 1);
    CHECK(op::invocations() == before + 1);
}

TEST_CASE("SOURCE-1: the door quotes the refusal of whoever owns it, and invents none") {
    PaneRig r;
    const op::MountResult sourced =
        op::mount_provider(r.catalog, PROVIDER_SOURCE_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(sourced.ok, sourced.reason);
    const std::int64_t kind = open_powers(r);
    (void)kind;
    SampleBook book;
    const loom::WeaveId asker = seat_asker(r, book);

    // A PARAMETERIZED OPERATOR: the Source seam's own sentence, verbatim, naming the
    // ports sampling supplies none of.
    ask_for(r, asker, book, "prov.source.doubled");
    REQUIRE(book.heard.size() == 1);
    CHECK_FALSE(book.heard[0].ok);
    CHECK(book.heard[0].lines.empty());
    CHECK(book.heard[0].reason.find("is an operator and not a source") != std::string::npos);
    CHECK(book.heard[0].reason.find("(value)") != std::string::npos);
    CHECK(book.heard[0].reason == op::sample(r.catalog, "prov.source.doubled").reason());

    // AN IDENTITY NOBODY SUPPLIES: the CATALOG's own sentence, reached by resolving
    // again rather than by re-wording it anywhere.
    ask_for(r, asker, book, "nothing.supplies.this");
    REQUIRE(book.heard.size() == 2);
    CHECK_FALSE(book.heard[1].ok);
    CHECK(book.heard[1].reason == op::sample(r.catalog, "nothing.supplies.this").reason());
    CHECK_FALSE(book.heard[1].reason.empty());

    // AND A SOURCE THAT DISAPPEARED BETWEEN DISPLAY AND SPEND GETS THAT SAME SENTENCE,
    // because the door resolves at the spend and never before it.
    ask_for(r, asker, book, "prov.source.spends");
    REQUIRE(book.heard.size() == 3);
    CHECK(book.heard[2].ok);
    REQUIRE(r.catalog.unmount("zengine.provider.source"));
    ask_for(r, asker, book, "prov.source.spends");
    REQUIRE(book.heard.size() == 4);
    CHECK_FALSE(book.heard[3].ok);
    CHECK(book.heard[3].reason == op::sample(r.catalog, "prov.source.spends").reason());
}

TEST_CASE("SOURCE-1: a cross-image Source proves the sample is an evaluation, not a lookup") {
    // ⭐ THE INSTRUMENT THE HOST'S COUNTER CANNOT BE. `op::invocations()` is a
    // vague-linkage static, so a body running inside the provider's own image is
    // invisible to it. This Source counts its own spends and answers 1, then 2, then 3
    // -- so a memoised sample, a cached answer or a re-shown retained value would all
    // read as a repeated number.
    PaneRig r;
    const op::MountResult sourced =
        op::mount_provider(r.catalog, PROVIDER_SOURCE_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(sourced.ok, sourced.reason);
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    focus_pane(r, kind);
    select_power(r, kind, "prov.source.spends");

    std::vector<std::int64_t> answers;
    for (int i = 0; i < 3; ++i) {
        r.key(input::scan::kReturn);
        answers.push_back(shown_count(pane_rows(r, kind)));
    }
    REQUIRE(answers.size() == 3);
    CHECK(answers[0] > 0);
    CHECK(answers[1] == answers[0] + 1);
    CHECK(answers[2] == answers[1] + 1);

    // AND REPAINTING BETWEEN TWO SAMPLES CHANGES NOTHING, which is what says the
    // retained answer is presentation rather than a subscription: a pane that re-asked
    // on every grant would show a rising number with nobody having gestured.
    r.extent(150, 44);
    CHECK(shown_count(pane_rows(r, kind)) == answers[2]);
    r.extent(160, 48);
    CHECK(shown_count(pane_rows(r, kind)) == answers[2]);
}

TEST_CASE("SOURCE-1: a retained sample is history, and an unload does not erase it") {
    PaneRig r;
    const op::MountResult sourced =
        op::mount_provider(r.catalog, PROVIDER_SOURCE_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(sourced.ok, sourced.reason);
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    focus_pane(r, kind);
    select_power(r, kind, "prov.source.spends");
    r.key(input::scan::kReturn);
    const std::int64_t answered = shown_count(pane_rows(r, kind));
    REQUIRE(answered > 0);

    // ---- THE PROVIDER GOES AWAY, AND THE ANSWER DOES NOT ----------------------
    //
    // ⭐ The sample was true when it was given. A later reading of the catalog is a
    // fact about the CATALOG, and erasing a maker's answer because the population
    // moved would be reinterpreting history from a fact that is not about it.
    REQUIRE(r.catalog.unmount("zengine.provider.source"));
    r.extent(150, 44); // a fresh reading, which is this tool's one beat
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK_FALSE(any_row(shown, "> prov.source.spends"));  // gone from the list
        CHECK(any_row(shown, intro::kSampledWhenAsked));       // and still on the pane
        CHECK(shown_count(shown) == answered);
        CHECK(any_row(shown, "prov.source.spends"));           // ...labelled with whose it was
    }

    // ---- AND A RE-SAMPLE SHOWS THE CURRENT REFUSAL ---------------------------
    //
    // The maker asks again, and what they get is the catalog's own sentence about the
    // identity NOW -- not the old answer, and not a second wording of the refusal.
    SampleBook book;
    const loom::WeaveId asker = seat_asker(r, book);
    ask_for(r, asker, book, "prov.source.spends");
    REQUIRE(book.heard.size() == 1);
    CHECK_FALSE(book.heard[0].ok);
    CHECK(book.heard[0].reason == op::sample(r.catalog, "prov.source.spends").reason());
}

TEST_CASE("SOURCE-1: the query is typed, edited, copied and pasted through the shipped seams") {
    PaneRig r;
    SkinSeat* skin = r.mount_skin_seat(); // the medium that owns the platform clipboard
    REQUIRE(skin != nullptr);
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    focus_pane(r, kind);

    // TYPED TEXT IS THE QUERY. There is one editable field, so a printable character
    // has exactly one place it could go and no gesture activates it.
    r.text("rec");
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(shown[0].find("find:rec") != std::string::npos);
        CHECK(any_row(shown, kRecipeCatalogSource));
        CHECK_FALSE(any_row(shown, kProjectAnchorSource));
        CHECK(shown[0].find("/1") != std::string::npos); // the filtered population
    }

    // ORDINARY EDITING, through `TextBox::consume` -- the component the Composer already
    // ships, not a fifth copy of an editing switch.
    r.key(input::scan::kBackspace);
    CHECK(pane_rows(r, kind)[0].find("find:re") != std::string::npos);

    // ⭐ AND A NON-ADMISSIBLE CHUNK IS REFUSED WHOLE. The row contract is printable
    // ASCII; a maker who typed `naive` with a diaeresis gets NONE of it rather than a
    // mangled half, and the pane keeps speaking rather than losing a whole update.
    r.text("na\xC3\xAFve");
    CHECK(pane_rows(r, kind)[0].find("find:re") != std::string::npos);
    CHECK(pane_rows(r, kind)[0].find("na") == std::string::npos);

    // THE CLIPBOARD CONVERSATION, both directions.
    r.key(input::scan::kA, input::mod::kCtrl);
    r.key(input::scan::kC, input::mod::kCtrl);
    // THE COPY REACHED THE MEDIUM THAT OWNS THE PLATFORM CLIPBOARD, which is the only
    // place a copy can honestly be said to have landed.
    CHECK(skin->platform == "re");

    // AND A PASTE IS A READ PERFORMED BECAUSE THE MAKER ASKED (QR-11): the Skin is
    // asked once, at the paste, and is never watched.
    const int reads_before = skin->clipboard_reads;
    r.key(input::scan::kEnd);
    r.key(input::scan::kV, input::mod::kCtrl);
    CHECK(skin->clipboard_reads == reads_before + 1);
    CHECK(pane_rows(r, kind)[0].find("find:rere") != std::string::npos);
}

TEST_CASE("SOURCE-1: keys routed to another pane cannot move the Powers pane") {
    // ⭐ WORKSHOP POINTS THE KEYBOARD AT THE PANE A MAKER LAST PRESSED INTO, and this
    // office offers three. A key meant for `loaded` or `arrangement` must not edit the
    // Powers query, switch its view or move its cursor -- and the guard is the first
    // line of every arm rather than a convention.
    PaneRig r;
    const std::int64_t powers = open_powers(r);
    // A SECOND PANE FROM THE SAME OFFICE, which is the sharpest form of the question:
    // one weave, two panes, and only one of them may be typed into. It is opened BEFORE
    // anything holds the keyboard, because `p` typed into a focused pane is a `p`
    // (MSG-0) and would not open the picker at all.
    r.pick(PaneRef{kIntroOffice, intro::kArrangementPane});
    REQUIRE(intro_row(r, intro::kArrangementPane) != nullptr);
    const std::int64_t project = intro_row(r, intro::kArrangementPane)->kind;

    focus_pane(r, powers);
    REQUIRE(keyboard_pane(r.session().panels) == powers);
    r.text("rec");
    const std::vector<std::string> mine = pane_rows(r, powers);
    REQUIRE(mine[0].find("find:rec") != std::string::npos);

    press_pane(r, project, 1, 1);
    REQUIRE(keyboard_pane(r.session().panels) == project); // the keyboard really moved

    r.text("ZZZ");
    r.key(input::scan::kTab);
    r.key(input::scan::kDown);
    r.key(input::scan::kReturn);

    const std::vector<std::string> after = pane_rows(r, powers);
    CHECK(after[0].find("find:rec") != std::string::npos);
    CHECK(after[0].find("ZZZ") == std::string::npos);
    CHECK(after[0].find("[Sources]") != std::string::npos);
    CHECK_FALSE(any_row(after, intro::kSampledWhenAsked));
}

TEST_CASE("SOURCE-1: a cold pane's first press is exactly one act") {
    // ⭐ THE PRESS THAT POINTS THE KEYBOARD AT A PANE IS AN ORDINARY PRESS, and the
    // provider cannot even tell it apart. So the protection has to be that no target
    // in this pane means two things -- which is asserted here through the live seam:
    // the very first press a maker ever makes selects, and runs nothing.
    PaneRig r;
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 18);
    const std::uint64_t before = op::invocations();
    const std::int64_t at = powers_row(r, kind, kProjectAnchorSource);
    REQUIRE(at >= 0);

    press_pane(r, kind, at, 1); // the FIRST press into this pane, ever
    CHECK(op::invocations() == before);
    const std::vector<std::string> shown = pane_rows(r, kind);
    CHECK(shown[static_cast<std::size_t>(at)].rfind(intro::kSelectedMark, 0) == 0);
    CHECK_FALSE(any_row(shown, intro::kSampledWhenAsked));

    // AND A PRESS BEFORE THE PANE HAS ANY PROJECTION AT ALL NAMES NOTHING -- the gap
    // between a room grant and its answer, in which Workshop shows `(waiting for the
    // provider)` and this pane holds no map to read a press against.
    PaneRig cold;
    cold.expose_host_sources();
    cold.mount_workshop();
    const load::Executed done = cold.run_plan(pane_plan());
    REQUIRE_MESSAGE(done.ok, done.refusal);
    cold.ready(); // NO arrangement door: the pane is granted room and never answered
    cold.extent(160, 48);
    REQUIRE(intro_row(cold, intro::kPowersPane) != nullptr);
    cold.pick(PaneRef{kIntroOffice, intro::kPowersPane});
    const std::int64_t waiting = intro_row(cold, intro::kPowersPane)->kind;
    for (std::int64_t row = 0; row < 6; ++row) {
        press_pane(cold, waiting, row, 0);
    }
    const ExternalPane* pane = cold.session().panels.external_pane(waiting);
    REQUIRE(pane != nullptr);
    CHECK(pane->awaiting);
    CHECK(pane->shown.empty());
}

TEST_CASE("SOURCE-1: THE LIVE MAKER WITNESS, end to end through the real pane") {
    // ⭐ THE WHOLE CAPABILITY, AS ONE SESSION. A maker opens Powers, meets the Sources
    // view, navigates, searches, switches to Operators, works there, comes back and
    // finds their place, filters by construction, samples a Source, sees an honest
    // answer, moves the world, samples again, and reads the difference.
    PaneRig r;
    const op::MountResult sourced =
        op::mount_provider(r.catalog, PROVIDER_SOURCE_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(sourced.ok, sourced.reason);
    const std::int64_t kind = open_powers(r);
    make_taller(r, intro::kPowersPane, 20);

    // 1-2. OPEN POWERS, AND MEET THE SOURCES VIEW.
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        REQUIRE_FALSE(shown.empty());
        CHECK(shown[0].find("[Sources]") != std::string::npos);
        CHECK(any_row(shown, kProjectAnchorSource));
        CHECK(any_row(shown, kRecipeCatalogSource));
        CHECK(any_row(shown, "prov.source.spends"));
        CHECK(shown[0].find("-/3") != std::string::npos);
    }

    // 3. NAVIGATE, and watch the position marker follow.
    focus_pane(r, kind);
    r.key(input::scan::kDown);
    CHECK(pane_rows(r, kind)[0].find("1/3") != std::string::npos);
    r.key(input::scan::kDown);
    r.key(input::scan::kDown);
    CHECK(pane_rows(r, kind)[0].find("3/3") != std::string::npos);
    r.key(input::scan::kUp);
    CHECK(pane_rows(r, kind)[0].find("2/3") != std::string::npos);

    // 4. SEARCH.
    r.text("anchor");
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(shown[0].find("find:anchor") != std::string::npos);
        CHECK(any_row(shown, kProjectAnchorSource));
        CHECK_FALSE(any_row(shown, "prov.source.spends"));
    }
    select_power(r, kind, kProjectAnchorSource);
    for (int i = 0; i < 6; ++i) {
        r.key(input::scan::kBackspace);
    }

    // 5-6. SWITCH TO OPERATORS AND WORK THERE INDEPENDENTLY.
    r.key(input::scan::kTab);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(shown[0].find("[Operators]") != std::string::npos);
        CHECK(any_row(shown, "math.max"));
        CHECK_FALSE(any_row(shown, kProjectAnchorSource));
    }
    r.text("select");
    select_power(r, kind, "logic.select_int");
    CHECK(pane_rows(r, kind)[0].find("1/1") != std::string::npos);
    for (int i = 0; i < 6; ++i) {
        r.key(input::scan::kBackspace);
    }

    // 7. SWITCH BACK, AND THE SOURCE IDENTITY IS STILL THE MAKER'S PLACE.
    r.key(input::scan::kTab);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(shown[0].find("[Sources]") != std::string::npos);
        const std::int64_t at = row_with_text(shown, kProjectAnchorSource);
        REQUIRE(at >= 0);
        CHECK(shown[static_cast<std::size_t>(at)].rfind(intro::kSelectedMark, 0) == 0);
    }
    // ...and the Operators view kept its own, independently.
    r.key(input::scan::kTab);
    {
        const std::int64_t at = row_with_text(pane_rows(r, kind), "logic.select_int");
        REQUIRE(at >= 0);
        CHECK(pane_rows(r, kind)[static_cast<std::size_t>(at)].rfind(intro::kSelectedMark, 0) ==
              0);
    }
    r.key(input::scan::kTab);

    // 8. TOGGLE COMPOSITE-ONLY against a catalog with none, and back.
    const std::int64_t control =
        chrome_column(r, kind, "[ ] " + std::string(intro::kCompositeWord));
    REQUIRE(control >= 0);
    press_pane(r, kind, 0, control);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(shown[0].find("[x] " + std::string(intro::kCompositeWord)) != std::string::npos);
        CHECK(any_row(shown, "3 sources here"));
        CHECK(any_row(shown, "hidden by the current filter"));
    }
    press_pane(r, kind, 0, control);

    // 9-11. SELECT THE ANCHOR AND SAMPLE IT, and read an honest rendered answer.
    select_power(r, kind, kProjectAnchorSource);
    const std::uint64_t before = op::invocations();
    r.key(input::scan::kReturn);
    CHECK(op::invocations() == before + 1);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK(any_row(shown, intro::kSampledWhenAsked));
        CHECK(any_row(shown, kProjectAnchorSource));
        CHECK(any_row(shown, "zengine.ProjectAnchor v1"));
        CHECK(any_row(shown, "anchor  \"/zen/pane-rig\""));
    }

    // 12-13. MOVE THE WORLD AND SAMPLE AGAIN. The retained answer did NOT follow the
    // owner on its own -- it is historical presentation -- and the new one did.
    r.project_anchor = "/zen/somewhere-else";
    CHECK(any_row(pane_rows(r, kind), "anchor  \"/zen/pane-rig\"")); // still the old one
    r.extent(150, 44);
    CHECK(any_row(pane_rows(r, kind), "anchor  \"/zen/pane-rig\"")); // a repaint is not a re-ask
    r.key(input::scan::kReturn);
    CHECK(op::invocations() == before + 2);
    CHECK(any_row(pane_rows(r, kind), "anchor  \"/zen/somewhere-else\""));

    // 14-16. UNLOAD A SAMPLED FIXTURE SOURCE, KEEP THE HISTORY, AND RE-SAMPLE.
    select_power(r, kind, "prov.source.spends");
    r.key(input::scan::kReturn);
    const std::int64_t said = shown_count(pane_rows(r, kind));
    CHECK(said > 0);
    REQUIRE(r.catalog.unmount("zengine.provider.source"));
    r.extent(160, 48);
    {
        const std::vector<std::string> shown = pane_rows(r, kind);
        CHECK_FALSE(any_row(shown, "> prov.source.spends"));
        CHECK(shown_count(shown) == said); // the answer that was true when it was asked
    }
    SampleBook book;
    const loom::WeaveId asker = seat_asker(r, book);
    ask_for(r, asker, book, "prov.source.spends");
    REQUIRE(book.heard.size() == 1);
    CHECK_FALSE(book.heard[0].ok);
    CHECK(book.heard[0].reason == op::sample(r.catalog, "prov.source.spends").reason());
}

// ---- QR-18: the Powers list past its window is reached by the wheel ----------------------

TEST_CASE("QR-18/SC-5: the Powers list past its window is reached by the wheel, through the "
          "real pane") {
    // ⚔ MUTATION (F6): dropping `PaneWheel` from Workshop's send, from the grant, or from
    // the provider's accept-set -- the marker below never leaves `-/N` and no hidden row
    // arrives. A source grep would not notice a grant; this witness does.
    PaneRig r;
    const op::MountResult sourced =
        op::mount_provider(r.catalog, PROVIDER_SOURCE_SO, op::MountMode::Ordinary);
    REQUIRE_MESSAGE(sourced.ok, sourced.reason);
    const std::int64_t kind = open_powers(r);

    // THE OPERATORS VIEW, in a pane short enough to window its three operators: six cells is
    // a chrome row and two list rows, one of them an entry and the other the marker.
    make_taller(r, intro::kPowersPane, 6);
    const std::int64_t ops = chrome_column(r, kind, "Operators");
    REQUIRE(ops >= 0);
    press_pane(r, kind, 0, ops);
    REQUIRE(pane_rows(r, kind)[0].find("[Operators]") != std::string::npos);
    press_outside(r, kind); // the keyboard back to Workshop: this is about the pointer
    REQUIRE(r.session().panels.keyboard == kNoPaneKind);
    const std::vector<std::string> start = pane_rows(r, kind);
    REQUIRE(any_row(start, " more"));
    // The position marker `k/N` on the chrome row: nothing selected yet, N operators.
    const std::size_t slash = start[0].find('/');
    REQUIRE(slash != std::string::npos);
    REQUIRE(start[0][slash - 1] == '-');
    const std::int64_t total = std::stoll(start[0].substr(slash + 1));
    REQUIRE(total > 2);

    // WHEEL DOWN, ONE NOTCH AT A TIME, TO THE TAIL. The row the cursor ends on was not on the
    // screen when it began: that is the hidden row the marker stood for.
    const ui::Rect body = external_body_rect(r.session(), kind);
    const std::int64_t cx = body.x + 4;
    const std::int64_t cy = body.y + kExternalHeaderRows + 1; // the list's first row
    for (std::int64_t i = 0; i < total + 1; ++i) {
        r.wheel_cell(-1.0, cx, cy);
    }
    const std::vector<std::string> end = pane_rows(r, kind);
    std::string chosen;
    for (const std::string& row : end) {
        if (row.rfind(intro::kSelectedMark, 0) == 0) {
            chosen = row;
        }
    }
    REQUIRE_FALSE(chosen.empty());
    CHECK_FALSE(any_row(start, chosen)); // reached, and it was hidden at the start
    CHECK(end[0].find(std::to_string(total) + "/" + std::to_string(total)) != std::string::npos);
    CHECK_FALSE(any_row(end, " more below"));
    CHECK(r.session().panels.keyboard == kNoPaneKind); // looking pointed no keys
    // AT THE TAIL A FURTHER NOTCH SAYS NOTHING: the content is identical, no frame is owed.
    r.wheel_cell(-1.0, cx, cy);
    CHECK(pane_rows(r, kind) == end);
    // AND BACK UP, in halves -- fractional notches are carried, whole ones spent.
    for (int i = 0; i < 2 * total; ++i) {
        r.wheel_cell(+0.5, cx, cy);
    }
    CHECK(pane_rows(r, kind)[0].find("1/" + std::to_string(total)) != std::string::npos);
    // THE HEADER ROW OF THE PANE SENDS NOTHING: the marker does not move.
    r.wheel_cell(-1.0, body.x + 4, body.y);
    CHECK(pane_rows(r, kind)[0].find("1/" + std::to_string(total)) != std::string::npos);
}
