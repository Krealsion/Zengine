// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- WHAT A BUILD SAID, READ WHERE THE BUILD WAS ASKED FOR.
//
// THIS FILE OWNS the output reader: a build's lines reach the REAL Builder tool as a runner says
// them, the tool keeps them by operation, the REAL Builder pane image asks for a page and shows
// it, and the REAL Workshop weave judges every row that pane publishes. The runner is a seat a
// case scripts -- what these cases measure is what the tool keeps and what a maker is shown, and
// the runner's own bytes-in-order claim is `test_builder.cpp`'s, against a real child process.
//
// WHAT A CASE MAY ASSERT is what a maker can see (the rows Workshop accepted from the pane, the
// declared keys) and what crossed the bus (the build asked for).

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "builder-pane/vocabulary.hpp"
#include "builder/recipe.hpp"
#include "builder/vocabulary.hpp"
#include "builder/weave.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/pane_text.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace {

namespace bld = zengine::builder;
namespace bpane = zengine::builder_pane;

inline PaneRef builder_ref() { return PaneRef{bpane::kBuilderPaneRole, bpane::kBuilderPane}; }

/// A COMPILER'S FAILURE, AS GCC IN A UTF-8 LOCALE WRITES IT under a build tool: a status line, the
/// build tool's FAILED line, a command echo of several kilobytes, the diagnostic with its quotes
/// (U+2018 and U+2019), the caret lines, an MSVC line ending CR LF, and the build tool's verdict.
const std::string kEcho = [] {
    std::string echo = "/usr/bin/c++";
    for (int i = 0; i < 60; ++i) {
        echo += " -I/home/maker/zen checkout/include/a/path/a/command/echo/carries";
    }
    return echo + " -c /home/maker/zen checkout/attention-pane/pane.cpp";
}();
const std::string kDiagnostic =
    "[1/2] Building CXX object attention-pane/CMakeFiles/zengine-attention-pane.dir/pane.cpp.o\n"
    "FAILED: attention-pane/CMakeFiles/zengine-attention-pane.dir/pane.cpp.o \n" + kEcho + "\n"
    "/home/maker/zen checkout/attention-pane/pane.cpp: In member function "
    "\xE2\x80\x98void {anonymous}::AttentionPaneWeave::say_view(Push&&)\xE2\x80\x99:\n"
    "/home/maker/zen checkout/attention-pane/pane.cpp:416:23: error: \xE2\x80\x98oops\xE2\x80\x99 "
    "was not declared in this scope\n"
    "  416 |         push(\"ATTENTION\" + oops);\n"
    "      |                            ^~~~\n"
    "pane.cpp(416): error C2065: 'oops': undeclared identifier\r\n"
    "ninja: build stopped: subcommand failed.\n";

/// THE RUNNER'S SEAT: it takes the tool's orders and, when a case says so, says a build's
/// observations to the Builder office as the runner does -- the start, the bytes in pieces, the
/// ending.
struct RunnerSeatState {
    std::int64_t orders = 0;
    ZEN_SHAPE(RunnerSeatState, 1, ZEN_FIELD(orders));
};

class RunnerSeat : public loom::WeaveBase<RunnerSeat, RunnerSeatState,
                                          loom::Accept<bld::RunBuild, SeatDo>,
                                          loom::Emit<bld::BuildStarted, bld::BuildOutput,
                                                     bld::BuildFinished>> {
public:
    std::vector<std::string> orders;
    std::function<void(loom::Mail&)> next;
    void on(const bld::RunBuild& order, loom::Mail&) {
        ++state_.orders;
        orders.push_back(order.recipe);
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = next;
            next = nullptr;
            what(mail);
        }
    }
};

/// THE REALIZATION OWNER'S VOICE, when a case wants a refusal: it hears the tool's offer and
/// answers that the project did not take it, in the owner's words.
struct RefusingBooterState {
    std::int64_t offers = 0;
    ZEN_SHAPE(RefusingBooterState, 1, ZEN_FIELD(offers));
};

class RefusingBooter
    : public loom::WeaveBase<RefusingBooter, RefusingBooterState,
                             loom::Accept<bld::OfferArtifact>, loom::Emit<bld::ArtifactRealized>> {
public:
    const RefusingBooterState& state() const { return state_; }
    void on(const bld::OfferArtifact& offer, loom::Mail& mail) {
        ++state_.offers;
        (void)mail.publish(bld::ArtifactRealized{
            offer.artifact, false,
            "the rebuilt '" + offer.artifact + "' changed a shape but kept its name and version",
            false});
    }
};

/// A LIVE WORKSHOP WITH THE REAL BUILDER TOOL AND THE REAL BUILDER PANE IMAGE, and a runner seat.
struct OutputRig {
    TempDir dir;
    PaneRig r;
    bld::BuilderWeave* tool = nullptr;
    RunnerSeat* runner = nullptr;
    loom::WeaveId runner_id{};
    std::int64_t kind = 0;
    ProjectFrontier frontier{};

    explicit OutputRig(const char* tag) : dir(tag) {
        r.host.project_dir = dir.path().generic_string();
        bld::Recipe recipe;
        recipe.id = "attention";
        recipe.artifact = "zengine-attention-pane";
        recipe.artifact_dir = "/zen/build/attention-pane";
        recipe.cmake_target =
            bld::CMakeTargetRecipe{"/zen/build", "zengine-attention-pane", "", "/zen/src/pane.cpp"};
        r.host_recipes.hold("/zen/runtime/development-build-recipes.json", {recipe},
                            &HostContext::so_in);

        loom::Grant order;
        order.allow_to_role(bld::RunBuild::zen_name, bld::RunBuild::zen_version,
                            bld::kBuildRunnerRole);
        order.allow_to_any(bld::BuildStatus::zen_name, bld::BuildStatus::zen_version);
        order.allow_to_any(bld::RecipeCatalog::zen_name, bld::RecipeCatalog::zen_version);
        order.allow_to_any(bld::OfferArtifact::zen_name, bld::OfferArtifact::zen_version);
        order.allow_to_any(bld::BuildOutputSaid::zen_name, bld::BuildOutputSaid::zen_version);
        auto held = std::make_unique<bld::BuilderWeave>(r.host_recipes.views(),
                                                        r.host_recipes.source());
        tool = held.get();
        const loom::WeaveId tool_id = r.bus.register_weave(std::move(held), std::move(order),
                                                           std::string(bld::kBuilderRole));
        tool->zen_set_self(tool_id);

        auto seat = std::make_unique<RunnerSeat>();
        runner = seat.get();
        loom::Grant say;
        for (const auto& shape :
             {std::pair{bld::BuildStarted::zen_name, bld::BuildStarted::zen_version},
              std::pair{bld::BuildOutput::zen_name, bld::BuildOutput::zen_version},
              std::pair{bld::BuildFinished::zen_name, bld::BuildFinished::zen_version}}) {
            say.allow_to_role(shape.first, shape.second, bld::kBuilderRole);
        }
        runner_id = r.bus.register_weave(std::move(seat), std::move(say),
                                         std::string(bld::kBuildRunnerRole));
        runner->zen_set_self(runner_id);
    }

    /// `with_presenter` PUTS THE SHIPPED PRESENTER IN THE PLAN, as a host's own plan row does: a
    /// menu this pane offers is granted to whoever holds `zengine.presenter`, and the reader's
    /// overflow route is exactly what that office presents.
    void open(std::int64_t width = 200, std::int64_t height = 56, bool with_presenter = false) {
        r.mount_workshop();
        r.host.frontier = [this] { return frontier; };
        auto door = std::make_unique<ProjectDoor>(r.host.project_dir,
                                                  (dir.path() / "marks.json").generic_string(),
                                                  r.host.frontier);
        ProjectDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        say.allow_to_any(ProjectFrontierSaid::zen_name, ProjectFrontierSaid::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(door), std::move(say), std::string(kProjectRole));
        raw->zen_set_self(id);

        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = bpane::kBuilderPaneStem;
        seat.weave = load::WeaveIntent{bpane::kBuilderPaneRole};
        plan.artifacts.push_back(seat);
        if (with_presenter) {
            load::ArtifactIntent presenter;
            presenter.stem = "zengine-menu-presenter";
            presenter.weave = load::WeaveIntent{kPresenterRole};
            plan.artifacts.push_back(presenter);
        }
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        const RuntimePane* offered = r.session().panels.runtime.find(bpane::kBuilderPaneRole,
                                                                     bpane::kBuilderPane);
        REQUIRE_MESSAGE(offered != nullptr, "the loaded image offered no `builder` pane");
        r.pick(builder_ref());
        kind = offered->kind;
        // A WIDTH AND HEIGHT OF ITS OWN, so a compiler's line and a reader's page fit in the room.
        const Written sized =
            author_pane_size(r.session().setup.active, builder_ref(),
                             PaneSize{pane_unit::kSubcells, subs(150)},
                             PaneSize{pane_unit::kSubcells, subs(24)});
        REQUIRE_MESSAGE(sized.accepted, sized.refusal);
        r.extent(width, height + 1);
        const ExternalPane* pane = seat_of();
        REQUIRE(pane != nullptr);
        REQUIRE(pane->columns >= 140);
        REQUIRE(pane->rows >= 20);
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y);
        REQUIRE(r.session().panels.keyboard == kind);
    }

    const ExternalPane* seat_of() { return r.session().panels.external_pane(kind); }

    /// THE ROWS WORKSHOP ACCEPTED FROM THE PANE, exactly as published -- or none, when it refused.
    std::vector<std::string> rows() {
        std::vector<std::string> out;
        const ExternalPane* pane = seat_of();
        REQUIRE(pane != nullptr);
        for (const surface::SurfaceTextRow& row : pane->shown) {
            out.push_back(row.text);
        }
        // A BUILDER PANE ALWAYS SHOWS ROWS, so none is a picture Workshop refused or never took --
        // said here as a failure, rather than left to a case's `rows()[0]` to crash on (the
        // spelling reversion did, and the crash skipped the rest of the suite).
        REQUIRE_MESSAGE(!out.empty(), "the Builder pane shows no rows: its picture was refused or never published");
        return out;
    }
    std::string text() {
        std::string all;
        for (const std::string& row : rows()) {
            all += row + '\n';
        }
        return all;
    }

    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
    }

    /// THE RUNNER SAYS ONE WHOLE BUILD: it started as `op`, said `bytes` in pieces of `piece`
    /// bytes, and exited with `status`.
    void runner_says(std::int64_t op, const std::string& recipe, const std::string& bytes,
                     std::int64_t status, std::size_t piece = 700) {
        runner->next = [op, recipe, bytes, status, piece](loom::Mail& mail) {
            (void)mail.send_to_role(bld::kBuilderRole,
                                    bld::BuildStarted{op, recipe, "cmake --build /zen/build"});
            for (std::size_t at = 0; at < bytes.size(); at += piece) {
                (void)mail.send_to_role(bld::kBuilderRole,
                                        bld::BuildOutput{op, recipe, bytes.substr(at, piece)});
            }
            (void)mail.send_to_role(bld::kBuilderRole, bld::BuildFinished{op, recipe, status});
        };
        (void)r.bus.send(runner_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                  loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// ONE BUILD, AS A MAKER ASKS FOR IT AND AS THE RUNNER ANSWERS IT: `b` in the pane, then the
    /// runner's observations for the operation the tool is now following.
    void build(std::int64_t op, const std::string& bytes, std::int64_t status) {
        letter(input::scan::kB, "b");
        REQUIRE_FALSE(runner->orders.empty());
        runner_says(op, runner->orders.back(), bytes, status);
    }

    std::vector<std::string> declared() {
        std::vector<std::string> ids;
        const RuntimePane* seat =
            r.session().panels.runtime.find(bpane::kBuilderPaneRole, bpane::kBuilderPane);
        REQUIRE(seat != nullptr);
        for (const v2::PaneActionRow& a : seat->actions) {
            ids.push_back(a.id);
        }
        return ids;
    }
};

bool every_byte_drawable(const std::vector<std::string>& rows) {
    for (const std::string& row : rows) {
        for (const char c : row) {
            const unsigned char b = static_cast<unsigned char>(c);
            if (b < 0x20u || b >= 0x7Fu) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

// ============================================================================
// The reader: a failed build's own words, on rows a canvas can draw
// ============================================================================

TEST_CASE("read output shows a failed build's own lines on rows Workshop takes, spelled in ASCII and said to be") {
    OutputRig o("out-read");
    o.open();
    o.build(1, kDiagnostic, 1);

    // THE BUILDER'S OWN FACE SAYS IT FAILED AND WHERE ITS WORDS ARE -- and Workshop took it: a
    // `said` row built from a compiler's non-ASCII line would otherwise refuse the whole picture.
    REQUIRE_MESSAGE(o.seat_of()->refusal.empty(), o.seat_of()->refusal_why);
    CHECK(o.text().find("FAILED -- op #1") != std::string::npos);
    CHECK(o.text().find("-- read output") != std::string::npos);

    o.letter(input::scan::kL, "l");
    const std::vector<std::string> rows = o.rows();
    REQUIRE_MESSAGE(o.seat_of()->refusal.empty(), o.seat_of()->refusal_why);
    CHECK(every_byte_drawable(rows));
    REQUIRE(rows.size() >= 10);
    // ONE HEADER: the operation, its recipe, how it ended, which lines, and what was spelled.
    CHECK(rows[0].rfind("output #1 attention -- FAILED, exit 1 -- lines 1-9 of 9", 0) == 0);
    CHECK(rows[0].find("4 characters spelled in ASCII") != std::string::npos);
    // ...THEN THE LINES, ONE ROW EACH, IN ORDER: the compiler's line whole, its quotes spelled.
    CHECK(rows[1].rfind("[1/2] Building CXX object", 0) == 0);
    CHECK(rows[2].rfind("FAILED: attention-pane/", 0) == 0);
    CHECK(rows[5] == "/home/maker/zen checkout/attention-pane/pane.cpp:416:23: error: 'oops' was "
                     "not declared in this scope");
    CHECK(rows[6] == "  416 |         push(\"ATTENTION\" + oops);");
    CHECK(rows[7] == "      |                            ^~~~"); // the caret stays under its column
    CHECK(rows[8] == "pane.cpp(416): error C2065: 'oops': undeclared identifier");
    CHECK(rows[9] == "ninja: build stopped: subcommand failed.");
    // THE COMMAND ECHO IS ONE ROW, CUT AT THE WIDTH WITH THE MARK -- not a screen of wrapped flags.
    const std::int64_t columns = o.seat_of()->columns;
    CHECK(static_cast<std::int64_t>(rows[3].size()) == columns);
    CHECK(rows[3].substr(rows[3].size() - 3) == "...");
    CHECK(rows[3].rfind("/usr/bin/c++ -I/home/maker/zen checkout/include", 0) == 0);

    // THE ARROWS PAN, AND THE HEADER SAYS FROM WHERE.
    o.r.key(input::scan::kRight);
    const std::vector<std::string> panned = o.rows();
    CHECK(panned[0].find("from column " + std::to_string(columns / 2 + 1)) != std::string::npos);
    CHECK(panned[3].rfind(kEcho.substr(static_cast<std::size_t>(columns / 2), 20), 0) == 0);
    o.r.key(input::scan::kLeft);
    CHECK(o.rows()[0].find("from column") == std::string::npos);

    // ...AND ESCAPE CLOSES IT: the build rows are the pane's again.
    const std::vector<std::string> reading = o.declared();
    CHECK(std::find(reading.begin(), reading.end(), bpane::kActionBuild) == reading.end());
    o.r.key(input::scan::kEscape);
    const std::vector<std::string> ids = o.declared();
    CHECK(std::find(ids.begin(), ids.end(), bpane::kActionBuild) != ids.end());
    CHECK(o.text().find("FAILED -- op #1") != std::string::npos);
}

TEST_CASE("a compiler's non-ASCII words in a build's last lines leave the Builder's picture standing") {
    // THE LAST LINE IS THE ONE WITH THE QUOTES, so the tool's `detail`, the notice and the `said`
    // rows all carry them. One byte a canvas cannot draw refuses a pane's whole publication
    // (`judge_content`); spelled, the picture stands and reads as the compiler meant it.
    OutputRig o("out-detail");
    o.open();
    o.build(1,
            "/home/maker/zen checkout/attention-pane/pane.cpp:416:23: error: "
            "\xE2\x80\x98oops\xE2\x80\x99 was not declared in this scope\n",
            1);
    REQUIRE_MESSAGE(o.seat_of()->refusal.empty(), o.seat_of()->refusal_why);
    CHECK(every_byte_drawable(o.rows()));
    CHECK(o.text().find("error: 'oops' was not declared") != std::string::npos);
}

TEST_CASE("the reader stays bound to its build: a newer build and a new status do not move it, and it steps between the builds kept") {
    OutputRig o("out-bound");
    o.open();
    o.build(1, kDiagnostic, 1);
    o.build(2, "[1/2] Building CXX object attention-pane/pane.cpp.o\n[2/2] Linking\n", 0);

    // OPENED NOW, IT IS ABOUT THE BUILD THE TOOL IS FOLLOWING: #2. Its artifact is not on disk
    // here, so the tool's word for it is NO ARTIFACT -- the reader carries that word as told.
    o.letter(input::scan::kL, "l");
    CHECK(o.rows()[0].rfind("output #2 attention -- NO ARTIFACT -- lines 1-2 of 2", 0) == 0);

    // `[` STEPS TO THE OLDER BUILD KEPT, AND SAYS A NEWER ONE EXISTS.
    o.letter(input::scan::kLeftBracket, "[");
    std::vector<std::string> rows = o.rows();
    CHECK(rows[0].rfind("output #1 attention -- FAILED, exit 1", 0) == 0);
    CHECK(rows[0].find("build #2 is newer") != std::string::npos);
    REQUIRE(rows.size() > 5);
    CHECK(rows[5].find("error: 'oops' was not declared") != std::string::npos);

    // A THIRD BUILD, ASKED BY SOMEBODY ELSE WHILE THIS READER IS OPEN, MOVES NOTHING IT SHOWS.
    (void)o.r.bus.send_to_role(bld::kBuilderRole,
                               loom::Message(loom::to_value(bld::BuildRequested{"attention"})));
    o.r.bus.drain_until_idle();
    o.runner_says(3, "attention", "[1/1] nothing to do\n", 0);
    rows = o.rows();
    CHECK(rows[0].rfind("output #1 attention -- FAILED, exit 1", 0) == 0);
    CHECK(rows[0].find("build #3 is newer") != std::string::npos);

    // `]` STEPS NEWER, AND PAST THE NEWEST IT SAYS SO.
    o.letter(input::scan::kRightBracket, "]");
    CHECK(o.rows()[0].rfind("output #2", 0) == 0);
    o.letter(input::scan::kRightBracket, "]");
    CHECK(o.rows()[0].rfind("output #3", 0) == 0);
    o.letter(input::scan::kRightBracket, "]");
    CHECK(o.text().find("no newer build's output is kept") != std::string::npos);
    CHECK(o.text().find("output #3") != std::string::npos);
}

TEST_CASE("a build the tool no longer keeps is said, and no other build's lines are shown under its number") {
    OutputRig o("out-gone");
    o.open();
    o.build(1, kDiagnostic, 1);
    o.letter(input::scan::kL, "l");
    REQUIRE(o.rows()[0].rfind("output #1", 0) == 0);

    // FOUR MORE BUILDS, ASKED ELSEWHERE, AND THE TOOL KEEPS FOUR: #1's words are let go.
    for (std::int64_t op = 2; op <= 5; ++op) {
        (void)o.r.bus.send_to_role(bld::kBuilderRole,
                                   loom::Message(loom::to_value(bld::BuildRequested{"attention"})));
        o.r.bus.drain_until_idle();
        o.runner_says(op, "attention", "build " + std::to_string(op) + "\n", 0);
    }
    o.r.key(input::scan::kHome);
    const std::vector<std::string> rows = o.rows();
    REQUIRE_FALSE(rows.empty());
    CHECK(rows[0].rfind("output #1 -- not kept any more: this Builder keeps the output of #2, #3, "
                        "#4, #5",
                        0) == 0);
    CHECK(o.text().find("build 5") == std::string::npos);
    CHECK(o.text().find("oops") == std::string::npos);
}

TEST_CASE("a build that worked and a realization that was refused read as two answers, and the reader says the build succeeded") {
    OutputRig o("out-refused");
    auto booter = std::make_unique<RefusingBooter>();
    RefusingBooter* raw = booter.get();
    loom::Grant say;
    say.allow_to_any(bld::ArtifactRealized::zen_name, bld::ArtifactRealized::zen_version);
    const loom::WeaveId booter_id = o.r.bus.register_weave(std::move(booter), std::move(say));
    raw->zen_set_self(booter_id);
    // THE PRODUCT IS ON DISK where the recipe says, as the tool requires before it offers.
    const std::filesystem::path artifacts = o.dir.path() / "artifacts";
    std::filesystem::create_directories(artifacts);
    {
        bld::Recipe recipe;
        recipe.id = "attention";
        recipe.artifact = "zengine-attention-pane";
        recipe.artifact_dir = artifacts.generic_string();
        recipe.cmake_target =
            bld::CMakeTargetRecipe{"/zen/build", "zengine-attention-pane", "", "/zen/src/pane.cpp"};
        o.r.host_recipes.hold("/zen/runtime/development-build-recipes.json", {recipe},
                              &HostContext::so_in);
        std::ofstream out(HostContext::so_in(artifacts.generic_string(), "zengine-attention-pane"),
                          std::ios::binary | std::ios::trunc);
        out << "image";
        REQUIRE(out.good());
    }
    o.open();
    // LOAD AFTER BUILD ON, then a build the runner says worked.
    o.r.key(input::scan::kB, input::mod::kShift);
    o.r.text("B");
    REQUIRE(o.text().find("load after build: on") != std::string::npos);
    o.build(1, "[2/2] Linking CXX shared library attention-pane/zengine-attention-pane.so\n", 0);

    const std::string face = o.text();
    CHECK(raw->state().offers == 1);
    CHECK(face.find("succeeded -- op #1") != std::string::npos);
    // IT PRODUCED ITS ARTIFACT, SO THE `last` ROW DOES NOT POINT AT THE BUILD'S OWN WORDS.
    // The claim lives on that row and not in the whole face: the pane's control strip carries
    // a `[read output #1]` button in every state, which is the mouse route to the reader and
    // says nothing about whether this build has something to explain.
    std::string last_row;
    for (const std::string& row : o.rows()) {
        if (row.rfind("last", 0) == 0) {
            last_row = row;
        }
    }
    REQUIRE_FALSE(last_row.empty());
    CHECK(last_row.find("read output") == std::string::npos);
    CHECK(face.find("REFUSED -- the rebuilt 'zengine-attention-pane' changed a shape") !=
          std::string::npos);

    o.letter(input::scan::kL, "l");
    CHECK(o.rows()[0].rfind("output #1 attention -- succeeded, exit 0 -- lines 1-1 of 1", 0) == 0);
}

// =============================================================================
// The reader by hand (WL-OUT-04, and the mouse work over it)
// =============================================================================

namespace {

struct OutFaceAt {
    std::int64_t row = -1;
    std::int64_t column = -1;
};
OutFaceAt out_face_at(const std::vector<std::string>& rows, const std::string& face) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const std::size_t at = rows[i].find(face);
        if (at != std::string::npos) {
            return OutFaceAt{static_cast<std::int64_t>(i), static_cast<std::int64_t>(at)};
        }
    }
    return OutFaceAt{};
}

void out_press_face(OutputRig& o, const std::string& face) {
    const OutFaceAt at = out_face_at(o.rows(), face);
    REQUIRE_MESSAGE(at.row >= 0, "no control read `", face, "` in\n", o.text());
    press_pane(o.r, o.kind, at.row, at.column + 1);
}

} // namespace

TEST_CASE("WL-OUT-04: a build's own words are opened, stepped, panned and closed by hand") {
    // A BUILD THAT FAILED IS WHERE A MAKER MOST NEEDS THE MOUSE. The reader is a mode with no
    // build verb in it -- by key and now by hand: every control it draws moves the view or
    // closes it, and the pane sends the tool nothing but a page while it is open.
    OutputRig o("out-mouse");
    o.open();
    const std::string said = "first line\nsecond line\nthird line\n" +
                             std::string(300, 'x') + "\nlast line\n";
    o.build(1, said, 2);
    REQUIRE_MESSAGE(out_face_at(o.rows(), "[read output #1]").row >= 0, o.text());
    const std::size_t orders = o.runner->orders.size();

    out_press_face(o, "[read output #1]");
    REQUIRE_MESSAGE(o.rows()[0].rfind("output #1", 0) == 0, o.text());
    // NO BUILD VERB IS DRAWN, so no slip of the hand can start one from the reader.
    CHECK(out_face_at(o.rows(), "[build]").row < 0);
    CHECK(out_face_at(o.rows(), "[close output]").row >= 0);

    out_press_face(o, "[last lines]");
    CHECK(o.rows()[0].rfind("output #1", 0) == 0);
    out_press_face(o, "[first line]");
    CHECK(o.rows()[0].find("lines 1-") != std::string::npos);
    out_press_face(o, "[down]");
    CHECK(o.rows()[0].find("lines 2-") != std::string::npos);
    out_press_face(o, "[up]");
    CHECK(o.rows()[0].find("lines 1-") != std::string::npos);

    // PANNING IS A CONTROL TOO, and the header says which column the view starts at.
    CHECK(out_face_at(o.rows(), "(pan left)").row >= 0); // nothing to the left yet
    out_press_face(o, "[pan right]");
    CHECK(o.rows()[0].find("from column") != std::string::npos);
    out_press_face(o, "[pan left]");
    CHECK(o.rows()[0].find("from column") == std::string::npos);

    // ...AND THE OLDER/NEWER STEPS SAY WHEN THERE IS NOTHING THERE.
    out_press_face(o, "[older build]");
    CHECK(o.rows()[0].find("no older build's output is kept") != std::string::npos);

    out_press_face(o, "[close output]");
    CHECK(o.rows()[0].rfind("output #", 0) != 0);
    CHECK(out_face_at(o.rows(), "[build]").row >= 0);
    CHECK(o.runner->orders.size() == orders); // the reader ordered no build
}

// =============================================================================
// The reader in a room too small for its strip (the review's fifth finding)
// =============================================================================

namespace {

/// A PICTURE, ONE NUMBERED ROW PER LINE -- what a failed menu case prints.
std::string out_picture(const std::vector<std::string>& rows) {
    std::string out;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        out += std::to_string(i) + "| " + rows[i] + (char)10;
    }
    return out;
}

/// THE ROWS OF THE MENU THE READER HAS OPEN.
std::vector<std::string> out_menu_rows(OutputRig& o) {
    REQUIRE(menu_shown(o.r.session()));
    return context_rows_on(o.r.last_canvas(), o.r.session());
}

/// OPEN THE PANE'S OWN MENU BY ITS CONTROL, and return what it offered.
std::vector<std::string> out_open_menu(OutputRig& o) {
    out_press_face(o, "[menu]");
    return out_menu_rows(o);
}

/// WALK THE PRESENTER'S CURSOR TO THE ROW READING `row` AND TAKE IT.
void out_choose_row(OutputRig& o, const std::string& row) {
    const std::int64_t at = presented_line_of(o.r.session(), row);
    REQUIRE_MESSAGE(at >= 0, "no menu row read `", row, "`");
    for (std::int64_t i = 0; i < at; ++i) {
        o.r.key(input::scan::kDown);
    }
    o.r.key(input::scan::kReturn);
}

/// A READER IN A ROOM TOO NARROW FOR ITS WHOLE STRIP.
void narrow(OutputRig& o) {
    const Written sized = author_pane_size(o.r.session().setup.active, builder_ref(),
                                           PaneSize{pane_unit::kSubcells, subs(30)},
                                           PaneSize{pane_unit::kSubcells, subs(9)});
    REQUIRE_MESSAGE(sized.accepted, sized.refusal);
    o.r.extent(199, 56); // a DIFFERENT extent, so the room is genuinely granted again
    const ExternalPane* pane = o.seat_of();
    REQUIRE(pane != nullptr);
    REQUIRE_MESSAGE(pane->columns < 60, "the pane was not narrowed: ", pane->columns);
}

} // namespace

TEST_CASE("WL-OUT-04: in a room too small for its strip the reader's whole list is in its own menu, and every row of it acts") {
    // ⭐ THE REVIEW'S FIFTH FINDING (B4), REPRODUCED AND REPAIRED. The strip drops what will not
    // fit and writes `+N in menu`, and the reader's menu offered `close` and `manage` alone --
    // so in a narrow room the pan, the two ends and the neighbouring builds were reachable by no
    // hand at all. The promise is kept in `offer_menu` or nowhere.
    //
    // (X) MUTATION, MEASURED. The reader's branch of `offer_menu` reduced to `kMenuClose` again:
    //   the four `CHECK`s on the offered rows fail, and the two choices below reach nothing.
    OutputRig o("out-narrow-menu");
    o.open(200, 56, /*with_presenter=*/true);
    o.build(1, "the first build said this\n", 1);
    o.build(2, "the second build said this\n", 0);
    o.letter(input::scan::kL, "l");
    REQUIRE_MESSAGE(o.rows()[0].rfind("output #2", 0) == 0, o.text());

    narrow(o);
    const std::vector<std::string> strip = o.rows();
    INFO("the narrow reader showed\n", o.text());
    // THE ROOM GENUINELY CANNOT SHOW THEM: this is the state the review reproduced.
    REQUIRE(out_face_at(strip, "[older build]").row < 0);
    REQUIRE(out_face_at(strip, "[pan right]").row < 0);
    REQUIRE(out_face_at(strip, "[menu]").row >= 0); // ...and the route is never dropped
    // ...AND IT COUNTS THEM. The count rides on the last strip row; this room is narrow enough
    // that the row itself is cut, so the claim is the count and not the whole sentence.
    bool says_overflow = false;
    for (const std::string& row : strip) {
        says_overflow = says_overflow || row.find("+7") != std::string::npos;
    }
    CHECK(says_overflow);

    const std::vector<std::string> offered = out_open_menu(o);
    INFO("the reader's menu offered\n", out_picture(offered));
    for (const char* row : {"a line up", "a line down", "the first line", "the last lines",
                            "pan left", "pan right", "the older build's output",
                            "the newer build's output", "close this build's output",
                            "manage this pane..."}) {
        CHECK_MESSAGE(any_row(offered, row), "the reader's menu has no row `", row, "`");
    }

    // ...AND EVERY ROW ACTS. The older build is a different operation's output, under its own
    // header, and the reader stays bound to it.
    out_choose_row(o, "the older build's output");
    CHECK_MESSAGE(o.rows()[0].rfind("output #1", 0) == 0, o.text());
    CHECK_MESSAGE(o.text().find("the first build said this") != std::string::npos, o.text());
    // PANNING MOVES THE LINE UNDER THE HEADER. In a room this narrow the header itself is cut,
    // so the claim is the LINE: the view starts further along it than it did.
    out_press_face(o, "[menu]");
    out_choose_row(o, "pan right");
    CHECK_MESSAGE(o.text().find("the first build said this") == std::string::npos, o.text());
    CHECK_MESSAGE(o.text().find("d said this") != std::string::npos, o.text());
    out_press_face(o, "[menu]");
    out_choose_row(o, "close this build's output");
    CHECK_MESSAGE(o.rows()[0].rfind("output #", 0) != 0, o.text());
}

TEST_CASE("WL-OUT-04: the reader offers its whole list even when it is drawing no lines at all") {
    // A STATE THE READER DRAWS NO LINES IN. A menu that only existed once lines were on the
    // screen would leave a maker stuck in exactly the state they most need a way out of. The
    // other such state -- a page the Builder has not answered yet -- is the Builder suite's,
    // where a fixture tool can be held silent (`BLD-MOUSE: a reader waiting on its first page`).
    OutputRig o("out-empty-menu");
    o.open(200, 56, /*with_presenter=*/true);
    o.build(1, "the first build said this\n", 1);

    SUBCASE("the lines are no longer kept") {
        for (std::int64_t op = 2; op <= 5; ++op) {
            o.build(op, "build " + std::to_string(op) + "\n", 0);
        }
        o.letter(input::scan::kL, "l");
        REQUIRE_MESSAGE(o.rows()[0].rfind("output #5", 0) == 0, o.text());
        // WALK BACK PAST WHAT THE TOOL KEEPS: the reader says so and still offers its rows.
        for (int i = 0; i < 6; ++i) {
            out_press_face(o, "[older build]");
        }
        INFO("the reader showed\n", o.text());
        narrow(o);
        const std::vector<std::string> offered = out_open_menu(o);
        INFO("offered\n", out_picture(offered));
        CHECK(any_row(offered, "the newer build's output"));
        CHECK(any_row(offered, "close this build's output"));
        out_choose_row(o, "close this build's output");
        CHECK_MESSAGE(o.rows()[0].rfind("output #", 0) != 0, o.text());
    }
}
