// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite -- A PANE'S CODE, REACHED FROM THE PANE.
//
// THIS FILE OWNS Edit Code: a maker points at a running pane and asks for its code; the host
// answers which artifact and recipes stand behind the pane's office; the desk opens the one
// recipe's source through the managed opening; and the Builder is told which recipe the opened
// source belongs to. Everything below runs through the REAL Workshop weave, the REAL opening
// manager, the REAL Editor and Builder pane images, and the REAL example pane a maker is given
// (`examples/tally-pane/tally.cpp`), realized from a plan by the real executor -- and the host's
// answer is wired over those owners exactly as `workshop.cpp` wires it.
//
// WHAT A CASE MAY ASSERT is what a maker can see (the notice, a pane's rows, the file the Editor
// holds) and what crossed the bus (the open asked for, the reading published). The one pure case
// asks the host-side join directly, because what it pins -- a WeaveId join, never a role string
// -- is only arrangeable there.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "builder-pane/vocabulary.hpp"
#include "builder/recipe.hpp"
#include "builder/vocabulary.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/pane_text.hpp"
#include "workshop/provenance.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace bld = zengine::builder;
namespace bpane = zengine::builder_pane;

constexpr const char* kTallyStem = "zengine-example-tally";
/// The office the example's source speaks as -- spelled here as a maker's plan row spells it.
constexpr const char* kTallyOffice = "example.tally";
constexpr const char* kEditorStem = "zengine-editor-pane";
constexpr const char* kEditorOffice = "zengine.editor";

inline PaneRef tally_ref() { return PaneRef{kTallyOffice, "tally"}; }
inline PaneRef builder_ref() { return PaneRef{bpane::kBuilderPaneRole, bpane::kBuilderPane}; }
inline PaneRef editor_ref() { return PaneRef{kEditorOffice, "editor"}; }

inline std::string spelled(const std::filesystem::path& at) {
    return persist::resolved_against(std::string(), at.generic_string());
}

inline bld::Recipe single_recipe(const std::string& id, const std::string& artifact,
                                 const std::string& source) {
    bld::Recipe out;
    out.id = id;
    out.artifact = artifact;
    bld::SingleSourceRecipe one;
    one.source = source;
    out.single_source = one;
    return out;
}

inline bld::Recipe target_recipe(const std::string& id, const std::string& artifact,
                                 const std::string& entry = std::string()) {
    bld::Recipe out;
    out.id = id;
    out.artifact = artifact;
    bld::CMakeTargetRecipe tree;
    tree.build_dir = "/project/build";
    tree.target = artifact;
    tree.entry = entry;
    out.cmake_target = tree;
    return out;
}

/// A STAND-IN FOR THE BUILDER TOOL: it answers the pane's status ask with the catalog a case
/// set and the picture it holds, keeps every build asked of it with its realize intention, and
/// says its picture again when a case settles a build. The real tool is `test_builder.cpp`'s.
struct CodeToolState {
    std::int64_t answered = 0;
    ZEN_SHAPE(CodeToolState, 1, ZEN_FIELD(answered));
};

class CodeTool
    : public loom::WeaveBase<CodeTool, CodeToolState,
                             loom::Accept<bld::StatusRequested, bld::BuildRequested, SeatDo>,
                             loom::Emit<bld::BuildStatus, bld::RecipeCatalog>> {
public:
    bld::RecipeCatalog catalog{};
    bld::BuildStatus status{};
    std::vector<std::string> builds;
    std::vector<bool> realizes;
    void on(const bld::StatusRequested&, loom::Mail& mail) {
        ++state_.answered;
        (void)mail.publish(catalog);
        (void)mail.publish(status);
    }
    /// A BUILD TAKEN IS NOT A BUILD SETTLED: the picture says `asked`, as the real tool's does, so
    /// the pane keeps watching until a case settles it on the tool's own beat.
    void on(const bld::BuildRequested& asked, loom::Mail& mail) {
        builds.push_back(asked.recipe);
        realizes.push_back(asked.realize);
        status = bld::BuildStatus{};
        status.recipe = asked.recipe;
        status.realize = asked.realize;
        status.builds = static_cast<std::int64_t>(builds.size());
        status.outcome = bld::outcome::kAsked;
        status.realization = asked.realize ? bld::realization::kAsked : bld::realization::kNotAsked;
        (void)mail.publish(status);
    }
    /// SAY THE PICTURE UNASKED -- how the real tool republishes once a build settles.
    void on(const SeatDo&, loom::Mail& mail) { (void)mail.publish(status); }
};

/// WHOEVER HEARS WHAT WORKSHOP PUBLISHES ABOUT AN OPENED PANE SOURCE, and as whom it was said.
struct SourceWatchState {
    std::int64_t heard = 0;
    ZEN_SHAPE(SourceWatchState, 1, ZEN_FIELD(heard));
};

class SourceWatch : public loom::WeaveBase<SourceWatch, SourceWatchState,
                                           loom::Accept<PaneSourceOpened, SeatDo>,
                                           loom::Emit<PaneSourceOpened>> {
public:
    std::vector<PaneSourceOpened> heard;
    std::vector<std::string> authors;
    /// What this seat should publish next, inside its own delivery -- the forgery lever.
    std::function<void(loom::Mail&)> next;
    void on(const PaneSourceOpened& said, loom::Mail& mail) {
        ++state_.heard;
        heard.push_back(said);
        authors.emplace_back(mail.authored_role());
    }
    void on(const SeatDo&, loom::Mail& mail) {
        if (next) {
            auto what = next;
            next = nullptr;
            what(mail);
        }
    }
};

/// AN OPENING OFFICE THAT TAKES THE ANSWER AWAY WITH IT and answers only when a case says.
struct HeldOpeningState {
    std::int64_t asked = 0;
    ZEN_SHAPE(HeldOpeningState, 1, ZEN_FIELD(asked));
};

class HeldOpening : public loom::WeaveBase<HeldOpening, HeldOpeningState,
                                           loom::Accept<OpenSourceRequested, SeatDo>,
                                           loom::Emit<SourceOpened>> {
public:
    std::vector<std::string> paths;
    loom::DeferredAnswer answer{};
    SourceOpened reply{true, std::string()};
    void on(const OpenSourceRequested& asked, loom::Mail& mail) {
        ++state_.asked;
        paths.push_back(asked.path);
        answer = mail.defer_answer();
    }
    void on(const SeatDo&, loom::Mail& mail) { (void)loom::answer_deferred(answer, mail, reply); }
};

/// A LIVE WORKSHOP WITH THE EDITOR, THE BUILDER PANE AND THE EXAMPLE PANE LOADED BY A PLAN, and
/// the host's answer about a pane's code wired over the rig's own bus, executor and catalog.
struct CodeRig {
    TempDir dir;
    std::filesystem::path root;
    /// What the read-only project door answers about realization's frontier, settable by a case.
    /// Declared BEFORE the rig, so the door the rig's bus owns never outlives what it reads.
    ProjectFrontier frontier{};
    std::string marks_path;
    PaneRig r;
    CodeTool* tool = nullptr;
    loom::WeaveId tool_id{};
    SourceWatch* watch = nullptr;
    loom::WeaveId watch_id{};
    DoorAsker* asker = nullptr;
    /// THE MAKER'S COPY OF THE EXAMPLE, inside the project: what a recipe names and what the
    /// Editor opens. The repository's own file is never the one a case edits.
    std::string source;

    explicit CodeRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
        std::filesystem::copy_file(TALLY_SOURCE, root / "tally.cpp");
        source = spelled(root / "tally.cpp");
        auto held = std::make_unique<CodeTool>();
        tool = held.get();
        loom::Grant say;
        say.allow_to_any(bld::BuildStatus::zen_name, bld::BuildStatus::zen_version);
        say.allow_to_any(bld::RecipeCatalog::zen_name, bld::RecipeCatalog::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(held), std::move(say), std::string(bld::kBuilderRole));
        tool->zen_set_self(id);
        tool_id = id;
    }

    /// THE LAST BUILD SETTLES: the tool's picture says how the build and its realization ended,
    /// about the artifact named, and the tool says it unasked, as the real one does.
    void settle(std::int64_t outcome, std::int64_t realization, const std::string& artifact) {
        tool->status.outcome = outcome;
        tool->status.realization = realization;
        tool->status.artifact = artifact;
        (void)r.bus.send(tool_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    /// EVERY BUILD THE TOOL WAS ASKED FOR, in order, with its intention -- the words of a failure.
    std::string sent() const {
        std::string out;
        for (std::size_t i = 0; i < tool->builds.size(); ++i) {
            out += (out.empty() ? "" : ", ") + tool->builds[i] +
                   (tool->realizes[i] ? " (realize)" : " (plain)");
        }
        return out.empty() ? std::string("none") : out;
    }

    /// A LETTER OF THE BUILDER'S OWN ROWS, as a backend reports one: the key, then its character.
    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
    }

    /// GIVE THE BUILDER AN AUTHORED WIDTH, so a sentence longer than its default room can be read
    /// whole; a new surface extent is the repaint that grants the pane its new room.
    void widen_builder(std::int64_t cells) {
        const Written wrote =
            author_pane_size(r.session().setup.active, builder_ref(),
                             PaneSize{pane_unit::kSubcells, subs(cells)}, PaneSize{});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        r.extent(cells + 60, 64);
        const ExternalPane* seat = r.session().panels.external_pane(kind_of(builder_ref()));
        REQUIRE(seat != nullptr);
        REQUIRE(seat->columns >= cells - 4);
    }

    /// THE BUILDER'S ROW THAT BEGINS WITH `head`, whole as painted -- or empty.
    std::string builder_row(const char* head) {
        for (const std::string& row : rows_of(builder_ref())) {
            if (row.rfind(head, 0) == 0) {
                return row;
            }
        }
        return std::string();
    }

    /// THE CATALOG IN FORCE: held by the host's recipe owner, and published by the tool as the
    /// pane's picture of it -- one list, two owners' views, as the host has it.
    void hold(std::vector<bld::Recipe> rows) {
        tool->catalog = bld::RecipeCatalog{};
        for (const bld::Recipe& row : rows) {
            tool->catalog.recipes.push_back(bld::RecipeSummary{row.id, row.artifact});
        }
        tool->catalog.source = "/project/build-recipes.json";
        r.host_recipes.hold("/project/build-recipes.json", std::move(rows), &HostContext::so_in);
    }

    /// `with_manager` false leaves the opening office unheld.
    void open(bool with_manager = true) {
        r.host.managed_pane = editor_ref();
        r.mount_workshop();
        if (with_manager) {
            r.mount_opening();
        }
        r.wire_code_source();
        mount_watch();
        mount_asker();
        mount_project_door();
        load::LoadPlan plan;
        plan.artifacts.push_back(row(kEditorStem, kEditorOffice));
        plan.artifacts.push_back(row(bpane::kBuilderPaneStem, bpane::kBuilderPaneRole));
        plan.artifacts.push_back(row(kTallyStem, kTallyOffice));
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(200, 64);
        REQUIRE_MESSAGE(r.session().panels.runtime.find(kTallyOffice, "tally") != nullptr,
                        "the example image offered no `tally` pane");
        r.pick(tally_ref());
        r.pick(builder_ref());
        widen_builder(126);
        REQUIRE(r.session().panels.has(kind_of(tally_ref())));
        REQUIRE(r.session().panels.has(kind_of(builder_ref())));
    }

    /// A PANE WHOSE OFFICE A NATIVE WEAVE HOLDS: a provider seat mounted on this bus, offering
    /// the `hello` pane and seated on the desk -- a holder no realization row ever minted.
    void seat_stranger() {
        ProviderSeat* seat = r.mount_provider(kHelloOffice);
        r.drive(seat, [](ProviderSeat& s, loom::Mail& m) { s.offer(m, good_offer()); });
        r.pick(hello_ref());
        REQUIRE(r.session().panels.has(kind_of(hello_ref())));
    }

    static load::ArtifactIntent row(const char* stem, const char* role) {
        load::ArtifactIntent out;
        out.stem = stem;
        out.weave = load::WeaveIntent{role};
        return out;
    }

    /// THE READ-ONLY PROJECT OFFICE, answering the frontier this rig holds -- the door the
    /// Builder pane asks what the project is waiting on.
    void mount_project_door() {
        r.host.frontier = [this] { return frontier; };
        marks_path = (root / "workshop-marks.json").generic_string();
        // ...AND IT ANSWERS WHICH FILE A RECIPE NAMES by whatever rule a case wired into
        // `host.recipe_source` -- none, unless a case wires the host's own.
        auto door = std::make_unique<ProjectDoor>(r.host.project_dir, marks_path, r.host.frontier,
                                                  ProjectDoor::Names{}, r.host.recipe_source);
        ProjectDoor* raw = door.get();
        loom::Grant say;
        say.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        say.allow_to_any(ProjectFrontierSaid::zen_name, ProjectFrontierSaid::zen_version);
        say.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
        const loom::WeaveId id =
            r.bus.register_weave(std::move(door), std::move(say), std::string(kProjectRole));
        raw->zen_set_self(id);
    }

    void mount_watch() {
        auto held = std::make_unique<SourceWatch>();
        watch = held.get();
        loom::Grant grant;
        grant.allow_to_any(PaneSourceOpened::zen_name, PaneSourceOpened::zen_version);
        watch_id = r.bus.register_weave(std::move(held), std::move(grant),
                                        std::string("zengine.test.source-watch"));
        watch->zen_set_self(watch_id);
    }

    void mount_asker() {
        auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
        asker = held.get();
        loom::Grant grant;
        grant.allow_to_any(OpenSourceRequested::zen_name, OpenSourceRequested::zen_version);
        grant.allow_to_any(SourceOpened::zen_name, SourceOpened::zen_version);
        grant.allow_to_any(loom::DispatchRefused::zen_name, loom::DispatchRefused::zen_version);
        const loom::WeaveId id = r.bus.register_weave(std::move(held), std::move(grant),
                                                      std::string(kDoorAskerOffice));
        asker->zen_set_self(id);
        asker->id = id;
    }

    void asker_says(std::function<void(DoorAsker&, loom::Mail&)> what) {
        asker->next = std::move(what);
        (void)r.bus.send(asker->id,
                         loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{}, loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    std::int64_t kind_of(const PaneRef& ref) {
        const std::optional<std::int64_t> kind = resolve_pane(ref, r.session().panels);
        REQUIRE(kind.has_value());
        return *kind;
    }

    /// POINT AT A PANE WITH THE SECOND BUTTON, ON ITS CHROME -- the host's menu opens on the
    /// pane, and nothing else moves. The chrome (the title row, `external_body_rect().y`) is the
    /// host's always; a press in the BODY is the pane's and empty by default, so the host menu
    /// for a doorless pane is reached here, not in its content (WL-CTX-08, empty by default).
    void point_at(const PaneRef& ref) {
        const ui::Rect body = external_body_rect(r.session(), kind_of(ref));
        r.right_press_cell(body.x + 1, body.y);
        REQUIRE(r.session().context.open);
        REQUIRE(r.session().context.subject == context_subject::kPane);
        REQUIRE(r.session().context.pane == ref);
    }

    /// WALK THE OPEN MENU TO `edit code` -- found by its id, so a reordering cannot misdirect
    /// the case. `settle` false queues the choosing key and returns: the case pumps.
    void choose_edit_code(bool settle = true) {
        const std::vector<ContextEntry> rows = context_population(context_subject::kPane, "");
        std::size_t at = rows.size();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            if (!rows[i].is_group && rows[i].row->act == Act::kEditCode) {
                at = i;
            }
        }
        REQUIRE(at < rows.size());
        for (std::size_t i = 0; i < at; ++i) {
            r.key(input::scan::kDown);
        }
        if (settle) {
            r.key(input::scan::kReturn);
            return;
        }
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::KeyPressed{input::scan::kReturn, "", input::mod::kNone}),
            loom::WeaveId{}, loom::WeaveId{}, 0));
    }

    /// PRESS INTO A PANE ON ITS FIRST BODY ROW -- the keys go there, and it is the selection.
    void press_into(const PaneRef& ref) {
        const std::int64_t kind = kind_of(ref);
        press_pane(r, kind, 0, 0);
        REQUIRE(r.session().panels.keyboard == kind);
    }

    std::vector<std::string> rows_of(const PaneRef& ref) { return pane_rows(r, kind_of(ref)); }

    std::string text_of(const PaneRef& ref) {
        std::string all;
        for (const std::string& one : rows_of(ref)) {
            all += one;
            all += '\n';
        }
        return all;
    }

    /// THE EDITOR'S STATUS ROW, which names the file it holds.
    std::string editor_status() {
        if (resolve_pane(editor_ref(), r.session().panels).has_value() &&
            r.session().panels.has(kind_of(editor_ref()))) {
            const std::vector<std::string> rows = rows_of(editor_ref());
            return rows.empty() ? std::string() : rows[0];
        }
        return std::string();
    }

    const std::string& notice() { return r.last_notice(); }
};

inline void put_bytes(const std::filesystem::path& at, const std::string& bytes) {
    std::ofstream out(at, std::ios::binary | std::ios::trunc);
    REQUIRE(out.good());
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.close();
    REQUIRE(out.good());
}

} // namespace

// ============================================================================
// The join: office -> holder -> realized artifact -> recipes, by the owners that know
// ============================================================================

TEST_CASE("the code behind an office is found by the weave holding it, never by a plan row's authored role") {
    // TWO REALIZED ROWS, AND THE TRAP BETWEEN THEM. Row `first` was AUTHORED as `office.a`;
    // row `second` holds weave #7. The office `office.a` is held -- right now -- by #7. A join
    // that compared role strings would answer `first`; the owners' own facts answer `second`.
    load::ResolvedArtifact first;
    first.stem = "first";
    first.weave_loaded = true;
    first.weave = loom::WeaveId{5};
    first.role = "office.a";
    load::ResolvedArtifact second;
    second.stem = "second";
    second.weave_loaded = true;
    second.weave = loom::WeaveId{7};
    second.role = "office.b";
    load::ResolvedArtifact supplier = second;
    supplier.stem = "supplier";
    supplier.weave = loom::WeaveId{9};
    supplier.provider_mounted = true;
    supplier.provider = "some.operators";
    const std::vector<load::ResolvedArtifact> rows{first, second, supplier};
    const std::vector<bld::Recipe> recipes{
        single_recipe("second-a", "second", "/project/second_a.cpp"),
        single_recipe("elsewhere", "first", "/project/first.cpp"),
        target_recipe("second-b", "second")};

    SUBCASE("the holder's own row answers, with every recipe producing its artifact in catalog order") {
        const HostContext::CodeSource code =
            provenance::code_source_of("office.a", loom::WeaveId{7}, rows, recipes);
        CHECK(code.office == "office.a");
        CHECK(code.weave == 7);
        CHECK(code.artifact == "second");
        CHECK(code.reload.empty());
        REQUIRE(code.recipes.size() == 2);
        CHECK(code.recipes[0].id == "second-a");
        CHECK(code.recipes[0].kind == "single_source");
        CHECK(code.recipes[0].source == "/project/second_a.cpp");
        CHECK(code.recipes[1].id == "second-b");
        CHECK(code.recipes[1].kind == "cmake_target");
        CHECK(code.recipes[1].source.empty()); // a tree and a target name no file; none is guessed
    }
    SUBCASE("nobody holding the office is an absence, and no row is consulted") {
        const HostContext::CodeSource code =
            provenance::code_source_of("office.a", loom::WeaveId{}, rows, recipes);
        CHECK(code.weave == 0);
        CHECK(code.artifact.empty());
        CHECK(code.recipes.empty());
    }
    SUBCASE("a holder no realized row minted is an absence of artifact, not the nearest row") {
        const HostContext::CodeSource code =
            provenance::code_source_of("office.b", loom::WeaveId{11}, rows, recipes);
        CHECK(code.weave == 11);
        CHECK(code.artifact.empty());
        CHECK(code.recipes.empty());
    }
    SUBCASE("a CMake target names the editing entry its author wrote, and one that wrote none names no file") {
        const std::vector<bld::Recipe> entries{target_recipe("tree-with", "second",
                                                             "/project/second/pane.cpp"),
                                               target_recipe("tree-without", "second")};
        const HostContext::CodeSource code =
            provenance::code_source_of("office.a", loom::WeaveId{7}, rows, entries);
        REQUIRE(code.recipes.size() == 2);
        CHECK(code.recipes[0].kind == "cmake_target");
        CHECK(code.recipes[0].source == "/project/second/pane.cpp");
        CHECK(code.recipes[1].kind == "cmake_target");
        CHECK(code.recipes[1].source.empty()); // nothing is guessed from the target or the stem
    }
    SUBCASE("an artifact that also supplies operators says why a rebuild cannot reload in place") {
        const HostContext::CodeSource code =
            provenance::code_source_of("office.c", loom::WeaveId{9}, rows, recipes);
        CHECK(code.artifact == "supplier");
        CHECK_FALSE(code.reload.empty());
        CHECK(code.reload == load::reload_refusal(supplier));
        CHECK(code.recipes.empty());
    }
}

// ============================================================================
// The loop's first half: point, Edit Code, the source is open, the Builder follows
// ============================================================================

TEST_CASE("Edit Code opens the pointed pane's one source through the opening office, and the Builder follows its recipe") {
    CodeRig c("code-open");
    // THE CATALOG'S OWN FIRST ROW IS NOT TALLY'S, so the Builder's standing row before the
    // gesture is `skin` -- and a Builder that moved for any other reason would not say `tally`.
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally", kTallyStem, c.source)});
    c.open();
    const std::int64_t tally = c.kind_of(tally_ref());
    const std::int64_t committed_before = c.r.opening->state().committed;

    // THE EXAMPLE IS WHAT THE WALKTHROUGH SAYS IT IS: pressed into, Space adds one.
    c.press_into(tally_ref());
    c.r.key(input::scan::kSpace);
    CHECK(c.text_of(tally_ref()).find("Tally: 1") != std::string::npos);

    // ...and the Builder, before anything, is on the catalog's own first row.
    REQUIRE(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);
    REQUIRE(c.text_of(builder_ref()).find("tally -> zengine-example-tally") == std::string::npos);

    c.point_at(tally_ref());
    c.choose_edit_code();

    // THE OPEN WENT THROUGH THE ONE MANAGER, AND IT TOOK.
    CHECK(c.r.opening->state().committed == committed_before + 1);
    CHECK(c.r.opening->state().last_path == c.source);
    CHECK(c.editor_status().find("tally.cpp") != std::string::npos);
    CHECK_FALSE(c.r.session().notice_is_bad);
    CHECK(c.notice().find("opened the source of Tally") != std::string::npos);
    CHECK(c.notice().find("recipe `tally`") != std::string::npos);
    CHECK(c.notice().find("reloads in place") != std::string::npos);

    // ONE READING, AS WORKSHOP'S OFFICE, NAMING THE CODE THE HOST FOUND.
    REQUIRE(c.watch->heard.size() == 1);
    CHECK(c.watch->authors[0] == kWorkshopProvider);
    CHECK(c.watch->heard[0].office == kTallyOffice);
    CHECK(c.watch->heard[0].pane == "tally");
    CHECK(c.watch->heard[0].name == "Tally");
    CHECK(c.watch->heard[0].artifact == kTallyStem);
    CHECK(c.watch->heard[0].recipe == "tally");
    CHECK(c.watch->heard[0].source == c.source);

    // THE BUILDER CHOSE THAT RECIPE, VISIBLY, AND BUILT NOTHING.
    const std::string builder = c.text_of(builder_ref());
    CHECK(builder.find("tally -> zengine-example-tally") != std::string::npos);
    CHECK(c.tool->builds.empty());

    // ...AND THE PANE ITSELF KEPT WHAT IT HAD: its count, its office, its seat.
    CHECK(c.text_of(tally_ref()).find("Tally: 1") != std::string::npos);
    CHECK(c.r.session().panels.has(tally));
}

TEST_CASE("Edit Code acts on the pane that was pointed at, not the selection, and a later selection redirects nothing") {
    CodeRig c("code-captured");
    c.hold({single_recipe("tally", kTallyStem, c.source)});
    c.open();
    const std::int64_t builder = c.kind_of(builder_ref());

    // THE BUILDER IS THE SELECTION AND HOLDS THE KEYS; THE MAKER POINTS AT TALLY.
    c.press_into(builder_ref());
    REQUIRE(c.r.session().panels.selected == builder);
    c.point_at(tally_ref());
    CHECK(c.r.session().panels.selected == builder); // pointing selected nothing

    SUBCASE("the pointed pane's code is the code asked for") {
        c.choose_edit_code();
        CHECK(c.r.opening->state().last_path == c.source);
        REQUIRE(c.watch->heard.size() == 1);
        CHECK(c.watch->heard[0].office == kTallyOffice);
    }
    SUBCASE("a press into another pane while the open is on its way changes nothing it opens") {
        c.choose_edit_code(/*settle=*/false);
        c.r.bus.pump_pending(); // the desk spends the choice and queues the one open
        press_pane(c.r, builder, 0, 0);
        c.r.bus.drain_until_idle();
        CHECK(c.r.opening->state().last_path == c.source);
        REQUIRE(c.watch->heard.size() == 1);
        CHECK(c.watch->heard[0].office == kTallyOffice);
        CHECK(c.watch->heard[0].recipe == "tally");
    }
}

TEST_CASE("a pointed pane that left the setup before Edit Code was chosen is refused, and nothing is asked") {
    CodeRig c("code-gone");
    c.hold({single_recipe("tally", kTallyStem, c.source)});
    c.open();
    const std::int64_t committed_before = c.r.opening->state().committed;
    const std::int64_t refused_before = c.r.opening->state().refused;
    c.point_at(tally_ref());
    REQUIRE(remove_pane(c.r.session().setup.active, tally_ref()));
    c.choose_edit_code();
    CHECK(c.r.session().notice_is_bad);
    CHECK(c.notice().find("no longer in this setup") != std::string::npos);
    CHECK(c.r.opening->state().committed == committed_before);
    CHECK(c.r.opening->state().refused == refused_before);
    CHECK(c.r.opening->state().last_path.empty());
    CHECK(c.watch->heard.empty());
}

// ============================================================================
// Absent, ambiguous and unavailable provenance: said, and nothing opened
// ============================================================================

TEST_CASE("code that cannot be named is said in words -- no recipe, several, a CMake target, a weave the plan did not load -- and nothing is opened") {
    SUBCASE("no recipe produces the artifact") {
        CodeRig c("code-norecipe");
        c.hold({target_recipe("skin", "zengine-skin")});
        c.open();
        c.point_at(tally_ref());
        c.choose_edit_code();
        CHECK(c.r.session().notice_is_bad);
        CHECK(c.notice().find("no build recipe produces zengine-example-tally") != std::string::npos);
        CHECK(c.notice().find("pick buildable") != std::string::npos);
        CHECK(c.r.opening->state().last_path.empty());
        CHECK(c.watch->heard.empty());
    }
    SUBCASE("several recipes produce it, and none is chosen for the maker") {
        CodeRig c("code-several");
        const std::filesystem::path other = c.root / "tally_debug.cpp";
        put_bytes(other, "int debug;\n");
        // THE FIRST CATALOG ROW IS A MATCH, which is what a "use the first" defect would take.
        c.hold({single_recipe("tally", kTallyStem, c.source),
                single_recipe("tally-debug", kTallyStem, spelled(other))});
        c.open();
        c.point_at(tally_ref());
        c.choose_edit_code();
        CHECK(c.r.session().notice_is_bad);
        CHECK(c.notice().find("2 recipes build zengine-example-tally") != std::string::npos);
        CHECK(c.notice().find("`tally`, `tally-debug`") != std::string::npos);
        CHECK(c.notice().find("choose one in the Builder") != std::string::npos);
        CHECK(c.r.opening->state().last_path.empty());
        CHECK(c.watch->heard.empty());
    }
    SUBCASE("the one recipe is a CMake target, and no source is guessed for it") {
        CodeRig c("code-target");
        c.hold({target_recipe("tally-tree", kTallyStem)});
        c.open();
        c.point_at(tally_ref());
        c.choose_edit_code();
        CHECK(c.r.session().notice_is_bad);
        CHECK(c.notice().find("`tally-tree` is a cmake_target recipe with no editing entry") !=
              std::string::npos);
        CHECK(c.notice().find("give it an `entry`") != std::string::npos);
        CHECK(c.r.opening->state().last_path.empty());
        CHECK(c.watch->heard.empty());
    }
    SUBCASE("the pane's office is held by a weave no plan row realized") {
        CodeRig c("code-stranger");
        c.hold({single_recipe("tally", kTallyStem, c.source)});
        c.open();
        c.seat_stranger();
        c.point_at(hello_ref());
        c.choose_edit_code();
        CHECK(c.r.session().notice_is_bad);
        CHECK(c.notice().find(std::string("Hello is drawn by ") + kHelloOffice) !=
              std::string::npos);
        CHECK(c.notice().find("this project's plan did not load") != std::string::npos);
        CHECK(c.r.opening->state().last_path.empty());
        CHECK(c.watch->heard.empty());
    }
    SUBCASE("the pane is Workshop's own") {
        CodeRig c("code-builtin");
        c.hold({single_recipe("tally", kTallyStem, c.source)});
        c.open();
        // THE LAYOUTS PANE: the one pane Workshop presents itself, in an office Workshop holds.
        // (It was the host's Pane Manager until that became the desktop's pane.) Pointed at on
        // its last cell, where no tab is, so the menu is the pane's and not a tab's.
        const PaneRef layouts{kWorkshopProvider, pane_key::kLayouts};
        if (!c.r.session().panels.has(c.kind_of(layouts))) {
            c.r.pick(layouts);
        }
        REQUIRE(c.r.session().panels.has(c.kind_of(layouts)));
        const ui::Rect slot = cells_covered(bounds_of(c.r.session().panels,
                                                      c.r.session().setup.active,
                                                      c.kind_of(layouts),
                                                      screen_of(c.r.session()))
                                                .rect);
        c.r.right_press_cell(slot.x + slot.w - 1, slot.y);
        REQUIRE(c.r.session().context.open);
        REQUIRE(c.r.session().context.pane == layouts);
        c.choose_edit_code();
        CHECK(c.r.session().notice_is_bad);
        CHECK(c.notice().find("is part of Workshop itself") != std::string::npos);
        CHECK(c.r.opening->state().last_path.empty());
        CHECK(c.watch->heard.empty());
    }
}

// ============================================================================
// The managed opening's own answers, read against the one ask
// ============================================================================

TEST_CASE("a dirty Editor refuses the pane's source, the refusal is said, and the Builder is not moved") {
    CodeRig c("code-dirty");
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally", kTallyStem, c.source)});
    c.open();
    // ANOTHER FILE IS OPEN, AND EDITED, AND NOT SAVED.
    const std::filesystem::path other = c.root / "notes.cpp";
    put_bytes(other, "int notes;\n");
    c.asker_says([path = spelled(other)](DoorAsker& a, loom::Mail& mail) {
        a.ask(mail, kOpeningRole, OpenSourceRequested{path});
    });
    REQUIRE(c.asker->opens.size() == 1);
    REQUIRE_MESSAGE(c.asker->opens[0].accepted, c.asker->opens[0].refusal);
    c.press_into(editor_ref());
    c.r.text("x");
    REQUIRE(c.editor_status().rfind("UNSAVED", 0) == 0);
    REQUIRE(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);

    c.point_at(tally_ref());
    c.choose_edit_code();
    CHECK(c.r.session().notice_is_bad);
    CHECK(c.notice().find("unsaved changes") != std::string::npos);
    CHECK(c.notice().find("Tally's code was not opened") != std::string::npos);
    CHECK(c.editor_status().find("notes.cpp") != std::string::npos);
    CHECK(c.editor_status().rfind("UNSAVED", 0) == 0);
    CHECK(c.watch->heard.empty());
    CHECK(c.text_of(builder_ref()).find("tally -> zengine-example-tally") == std::string::npos);
    CHECK(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);
}

TEST_CASE("an open no opening office could take is refused at dispatch by that attempt, and forged answers settle nothing") {
    CodeRig c("code-dispatch");
    c.hold({single_recipe("tally", kTallyStem, c.source)});
    c.open(/*with_manager=*/false);

    SUBCASE("Loom's word about the exact attempt is said") {
        c.point_at(tally_ref());
        c.choose_edit_code();
        CHECK(c.r.session().notice_is_bad);
        CHECK(c.notice().find("Tally's code was not opened") != std::string::npos);
        CHECK(c.notice().find(std::string("could not reach ") + kOpeningRole) != std::string::npos);
        CHECK(c.watch->heard.empty());
    }
    SUBCASE("a stranger's SourceOpened and a DispatchRefused naming the live attempt settle nothing") {
        auto held = std::make_unique<HeldOpening>();
        HeldOpening* opening = held.get();
        loom::Grant grant;
        grant.allow_to_any(SourceOpened::zen_name, SourceOpened::zen_version);
        const loom::WeaveId opening_id = c.r.bus.register_weave(std::move(held), std::move(grant),
                                                                std::string(kOpeningRole));
        opening->zen_set_self(opening_id);
        // THE RIGHT ATTEMPT NUMBER, read off the bus's own tap as the desk's ask is delivered,
        // so the forgery below names exactly what a real refusal would.
        std::uint64_t attempt = 0;
        const loom::WeaveId desk = c.r.workshop_id;
        const loom::ObserverId tap =
            c.r.bus.add_observer([&attempt, desk](const loom::BusEvent& ev) {
                if (ev.kind == loom::EventKind::Delivered && ev.sender == desk &&
                    ev.schema_name == OpenSourceRequested::zen_name) {
                    attempt = ev.seq;
                }
            });
        c.point_at(tally_ref());
        c.choose_edit_code();
        c.r.bus.remove_observer(tap);
        REQUIRE(opening->paths.size() == 1);
        REQUIRE(attempt != 0);
        const std::string pending = c.notice();
        REQUIRE(pending.find("opening the source of Tally") != std::string::npos);

        // THE FORGERIES, both as an office of their own: an accepted `SourceOpened` that answers
        // no ask of the desk's, and a `zen.DispatchRefused` naming the live attempt exactly.
        c.asker_says([desk, attempt](DoorAsker&, loom::Mail& mail) {
            // ...with the correlation of the desk's one ask (its first: this rig's only Edit
            // Code), so what refuses it is Loom's word that it is no answer, not a number.
            (void)mail.as_role(kDoorAskerOffice).send(desk, SourceOpened{true, std::string()}, 1);
            loom::DispatchRefused forged;
            forged.attempt = std::to_string(attempt);
            forged.role = kOpeningRole;
            forged.shape = OpenSourceRequested::zen_name;
            forged.version = 1;
            forged.reason = "NoSuchTarget";
            (void)mail.as_role(kDoorAskerOffice).send(desk, forged);
        });
        CHECK(c.notice() == pending);
        CHECK_FALSE(c.r.session().notice_is_bad);
        CHECK(c.watch->heard.empty());

        // THE REAL ANSWER STILL SETTLES IT.
        (void)c.r.bus.send(opening_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                     loom::WeaveId{}, 0));
        c.r.bus.drain_until_idle();
        CHECK(c.notice().find("opened the source of Tally") != std::string::npos);
        REQUIRE(c.watch->heard.size() == 1);
    }
}

TEST_CASE("an open that is never answered stays pending however many turns pass, and a late answer settles it") {
    CodeRig c("code-silent");
    c.hold({single_recipe("tally", kTallyStem, c.source)});
    c.open(/*with_manager=*/false);
    auto held = std::make_unique<HeldOpening>();
    HeldOpening* opening = held.get();
    loom::Grant grant;
    grant.allow_to_any(SourceOpened::zen_name, SourceOpened::zen_version);
    const loom::WeaveId opening_id =
        c.r.bus.register_weave(std::move(held), std::move(grant), std::string(kOpeningRole));
    opening->zen_set_self(opening_id);

    c.point_at(tally_ref());
    c.choose_edit_code();
    REQUIRE(opening->paths.size() == 1);
    CHECK(opening->paths[0] == c.source);
    const std::string pending = c.notice();
    CHECK(pending.find("opening the source of Tally") != std::string::npos);
    CHECK_FALSE(c.r.session().notice_is_bad);
    for (int turn = 0; turn < 256; ++turn) {
        c.r.bus.pump_pending();
    }
    c.r.key(input::scan::kUnknown); // an ordinary repaint, as the maker keeps working
    CHECK(c.notice() == pending);
    CHECK(c.watch->heard.empty());

    opening->reply = SourceOpened{false, "the opening of " + c.source + " was superseded"};
    (void)c.r.bus.send(opening_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                 loom::WeaveId{}, 0));
    c.r.bus.drain_until_idle();
    CHECK(c.r.session().notice_is_bad);
    CHECK(c.notice().find("superseded") != std::string::npos);
    CHECK(c.watch->heard.empty());
}

TEST_CASE("code that changed while its source was opening leaves the file open and tells the Builder nothing") {
    CodeRig c("code-moved");
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally", kTallyStem, c.source)});
    c.open();
    REQUIRE(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);
    c.point_at(tally_ref());
    c.choose_edit_code(/*settle=*/false);
    c.r.bus.pump_pending(); // the desk spends the choice and queues the one open
    REQUIRE(c.r.last_notice().find("opening the source of Tally") != std::string::npos);

    // A SECOND RECIPE FOR THE SAME ARTIFACT ARRIVES BEFORE THE ANSWER DOES.
    const std::filesystem::path other = c.root / "tally_debug.cpp";
    put_bytes(other, "int debug;\n");
    c.r.host_recipes.hold("/project/build-recipes.json",
                          {target_recipe("skin", "zengine-skin"),
                           single_recipe("tally", kTallyStem, c.source),
                           single_recipe("tally-debug", kTallyStem, spelled(other))},
                          &HostContext::so_in);
    c.r.bus.drain_until_idle();

    CHECK(c.editor_status().find("tally.cpp") != std::string::npos); // the open itself stands
    CHECK(c.r.session().notice_is_bad);
    CHECK(c.notice().find("changed while it opened") != std::string::npos);
    CHECK(c.notice().find("the Builder was not pointed at `tally`") != std::string::npos);
    CHECK(c.watch->heard.empty());
    CHECK(c.text_of(builder_ref()).find("tally -> zengine-example-tally") == std::string::npos);
}

// ============================================================================
// The Builder's half: it follows Workshop's reading, and nobody else's
// ============================================================================

TEST_CASE("the Builder pane follows an opened pane source only when Workshop's office said so") {
    CodeRig c("code-forged-reading");
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally", kTallyStem, c.source)});
    c.open();
    REQUIRE(c.text_of(builder_ref()).find("tally -> zengine-example-tally") == std::string::npos);

    // A STRANGER'S PUBLICATION OF THE SAME SHAPE, AS ITS OWN OFFICE.
    c.watch->next = [&c](loom::Mail& mail) {
        PaneSourceOpened forged;
        forged.office = kTallyOffice;
        forged.pane = "tally";
        forged.name = "Tally";
        forged.artifact = kTallyStem;
        forged.recipe = "tally";
        forged.source = c.source;
        (void)mail.as_role("zengine.test.source-watch").publish(forged);
    };
    (void)c.r.bus.send(c.watch_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                 loom::WeaveId{}, 0));
    c.r.bus.drain_until_idle();
    CHECK(c.text_of(builder_ref()).find("tally -> zengine-example-tally") == std::string::npos);
    CHECK(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);

    // ...AND WORKSHOP'S OWN, BY THE MAKER'S GESTURE, MOVES IT.
    c.point_at(tally_ref());
    c.choose_edit_code();
    CHECK(c.text_of(builder_ref()).find("tally -> zengine-example-tally") != std::string::npos);
}

TEST_CASE("the Builder's choice from Edit Code is not a pick between producers: the frontier action still asks") {
    CodeRig c("code-not-a-pick");
    c.hold({single_recipe("tally-a", kTallyStem, c.source)});
    c.open();
    c.point_at(tally_ref());
    c.choose_edit_code();
    REQUIRE(c.text_of(builder_ref()).find("tally-a -> zengine-example-tally") != std::string::npos);

    // A SECOND PRODUCER ARRIVES, AND THE PROJECT IS WAITING ON THE ARTIFACT BOTH PRODUCE.
    const std::filesystem::path other = c.root / "tally_b.cpp";
    put_bytes(other, std::string("int b;") + '\n');
    c.hold({single_recipe("tally-a", kTallyStem, c.source),
            single_recipe("tally-b", kTallyStem, spelled(other))});
    c.frontier = ProjectFrontier{true, kTallyStem, 0};
    c.widen_builder(148); // a new room: the Builder asks the tool again and hears both
    REQUIRE(c.text_of(builder_ref()).find("tally-a -> zengine-example-tally") != std::string::npos);

    // THE FRONTIER ACTION: several producers, and the one Edit Code chose is no pick of the maker's.
    c.press_into(builder_ref());
    c.r.key(input::scan::kF);
    c.r.text("f");
    CHECK(c.tool->builds.empty());
    CHECK(c.text_of(builder_ref()).find("2 recipes produce `zengine-example-tally`") !=
          std::string::npos);
}

TEST_CASE("a pick of another recipe does not follow Edit Code's choice: the frontier action still asks between producers") {
    CodeRig c("code-stale-pick");
    c.hold({single_recipe("tally-a", kTallyStem, c.source), target_recipe("skin", "zengine-skin")});
    c.open();

    // AN EXPLICIT PICK, BY THE BUILDER'S OWN `c`: from the catalog's first row to the skin.
    c.press_into(builder_ref());
    c.letter(input::scan::kC, "c");
    REQUIRE(c.text_of(builder_ref()).find("skin -> zengine-skin  (2/2)") != std::string::npos);

    // EDIT CODE ON TALLY: the open takes, and the Builder's choice moves to `tally-a`.
    c.point_at(tally_ref());
    c.choose_edit_code();
    REQUIRE(c.notice().find("opened the source of Tally") != std::string::npos);
    REQUIRE(c.text_of(builder_ref()).find("tally-a -> zengine-example-tally  (1/2)") !=
            std::string::npos);

    // A SECOND PRODUCER OF TALLY'S ARTIFACT JOINS THE RECIPES THERE WERE, AND THE PROJECT WAITS ON IT.
    const std::filesystem::path other = c.root / "tally_b.cpp";
    put_bytes(other, std::string("int b;") + '\n');
    c.hold({single_recipe("tally-a", kTallyStem, c.source), target_recipe("skin", "zengine-skin"),
            single_recipe("tally-b", kTallyStem, spelled(other))});
    c.frontier = ProjectFrontier{true, kTallyStem, 0};
    c.widen_builder(148); // a new room: the Builder asks the tool again and hears all three
    REQUIRE(c.text_of(builder_ref()).find("tally-a -> zengine-example-tally  (1/3)") !=
            std::string::npos);

    // THE PICK NAMED THE SKIN, AND THE CHOICE LEFT IT: nothing the maker picked stands between the two.
    c.press_into(builder_ref());
    c.letter(input::scan::kF, "f");
    INFO("builds sent: ", c.sent());
    CHECK(c.tool->builds.empty());
    CHECK(c.text_of(builder_ref()).find("2 recipes produce `zengine-example-tally` (`tally-a`, "
                                        "`tally-b`)") != std::string::npos);
}

TEST_CASE("a pick of the recipe Edit Code chose still stands: the frontier action builds it without another pick") {
    CodeRig c("code-same-pick");
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally-a", kTallyStem, c.source)});
    c.open();

    // THE MAKER PICKS `tally-a` THEMSELVES, and then Edit Code on Tally chooses the same recipe.
    c.press_into(builder_ref());
    c.letter(input::scan::kC, "c");
    REQUIRE(c.text_of(builder_ref()).find("tally-a -> zengine-example-tally  (2/2)") !=
            std::string::npos);
    c.point_at(tally_ref());
    c.choose_edit_code();
    REQUIRE(c.notice().find("opened the source of Tally") != std::string::npos);

    const std::filesystem::path other = c.root / "tally_b.cpp";
    put_bytes(other, std::string("int b;") + '\n');
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally-a", kTallyStem, c.source),
            single_recipe("tally-b", kTallyStem, spelled(other))});
    c.frontier = ProjectFrontier{true, kTallyStem, 0};
    c.widen_builder(148);
    REQUIRE(c.text_of(builder_ref()).find("tally-a -> zengine-example-tally  (2/3)") !=
            std::string::npos);

    // THE PICK NAMES THE RECIPE STILL CHOSEN, SO IT IS SPENT: one build, with the load aboard.
    c.press_into(builder_ref());
    c.letter(input::scan::kF, "f");
    REQUIRE(c.tool->builds.size() == 1);
    CHECK(c.tool->builds[0] == "tally-a");
    CHECK(c.tool->realizes[0]);
}

TEST_CASE("the Builder's words after Edit Code promise no reload: an owner's refusal stands alone, and an eligible pane still reads how to build it") {
    SUBCASE("the realization owner refuses a reload in place for this artifact") {
        CodeRig c("code-no-promise");
        c.hold({single_recipe("tally", kTallyStem, c.source)});
        c.open();
        // THE OWNER'S ANSWER, CONTROLLED: the real holder, artifact, recipe and source, with the
        // reload rule of a realized row that loaded a weave AND mounted an operator provider. The
        // example exports no provider; this is the owner's word supplied, not Tally's.
        const std::function<HostContext::CodeSource(const std::string&)> real =
            c.r.host.code_source;
        c.r.host.code_source = [real](const std::string& office) {
            HostContext::CodeSource code = real(office);
            if (!code.artifact.empty()) {
                load::ResolvedArtifact row;
                row.stem = code.artifact;
                row.weave_loaded = true;
                row.provider_mounted = true;
                code.reload = load::reload_refusal(row);
            }
            return code;
        };
        c.point_at(tally_ref());
        c.choose_edit_code();
        c.widen_builder(180);

        CHECK(c.notice().find("a rebuild will not reload in place: artifact "
                              "'zengine-example-tally' also supplies operators") !=
              std::string::npos);
        const std::string said = c.builder_row("build recipe: ");
        INFO("the Builder's row: ", said);
        INFO("Workshop's notice: ", c.notice());
        REQUIRE(said.find("build recipe: tally -> zengine-example-tally -- the source of Tally is "
                          "open in the Editor") != std::string::npos);
        CHECK(said.find(pane_text::kElided) == std::string::npos); // read whole
        CHECK(said.find("in place") == std::string::npos);
        CHECK(said.find("reloads") == std::string::npos);
        CHECK(c.tool->builds.empty());
    }
    SUBCASE("the ordinary eligible example") {
        CodeRig c("code-eligible-words");
        c.hold({single_recipe("tally", kTallyStem, c.source)});
        c.open();
        c.point_at(tally_ref());
        c.choose_edit_code();
        c.widen_builder(180);

        CHECK(c.notice().find("save, then build it in the Builder with load after build, and Tally "
                              "reloads in place") != std::string::npos);
        const std::string said = c.builder_row("build recipe: ");
        INFO("the Builder's row: ", said);
        CHECK(said.find(pane_text::kElided) == std::string::npos);
        CHECK(said.find("save it, then build (load after build: off)") != std::string::npos);
        CHECK(c.tool->builds.empty());
    }
}

TEST_CASE("load after build stays as the maker set it across Edit Code: the Builder says which, and the next loop's b alone offers its build") {
    CodeRig c("code-standing-arm");
    c.hold({single_recipe("tally", kTallyStem, c.source)});
    c.open();
    c.widen_builder(180);

    // THE FIRST LOOP OF A RUN: Edit Code, and the Builder says load after build is off.
    c.point_at(tally_ref());
    c.choose_edit_code();
    CHECK(c.builder_row("build recipe: ").find("(load after build: off)") != std::string::npos);
    c.press_into(builder_ref());
    c.r.key(input::scan::kB, input::mod::kShift);
    CHECK(c.builder_row("load after build: ").rfind("load after build: on", 0) == 0);
    c.letter(input::scan::kB, "b");
    REQUIRE(c.tool->realizes.size() == 1);
    CHECK(c.tool->realizes[0]);
    c.settle(bld::outcome::kSucceeded, bld::realization::kRealized, kTallyStem);
    // ...WHERE THE REALIZE ROW NOW SAYS WHAT THE LOAD DID, and not whether the switch is on.
    CHECK(c.builder_row("realize  ").rfind("realize  realized", 0) == 0);

    // THE NEXT LOOP: Edit Code again, and the Builder's words say the switch the maker left on.
    c.point_at(tally_ref());
    c.choose_edit_code();
    REQUIRE(c.notice().find("opened the source of Tally") != std::string::npos);
    CHECK(c.builder_row("build recipe: ").find("(load after build: on)") != std::string::npos);
    c.press_into(builder_ref());

    SUBCASE("b alone offers the build") {
        c.letter(input::scan::kB, "b");
        INFO("builds sent: ", c.sent());
        REQUIRE(c.tool->realizes.size() == 2);
        CHECK(c.tool->realizes[1]);
        // ...AND WHILE IT RUNS THE REALIZE ROW SAYS A LOAD WAS ASKED FOR.
        CHECK(c.builder_row("realize  ").rfind("realize  asked", 0) == 0);
    }
    SUBCASE("Shift+b first turns it off, says so, and b then only builds") {
        c.r.key(input::scan::kB, input::mod::kShift);
        CHECK(c.builder_row("load after build: ").rfind("load after build: off", 0) == 0);
        c.letter(input::scan::kB, "b");
        INFO("builds sent: ", c.sent());
        REQUIRE(c.tool->realizes.size() == 2);
        CHECK_FALSE(c.tool->realizes[1]);
        CHECK(c.builder_row("realize  ").rfind("realize  -- (load-after-build arms it)", 0) == 0);
    }
}

TEST_CASE("a finished build left unloaded keeps its button through Edit Code: it loads that build's own recipe, and b first makes it the chosen recipe's") {
    CodeRig c("code-ready-face");
    c.hold({target_recipe("skin", "zengine-skin"), single_recipe("tally", kTallyStem, c.source)});
    c.open();
    c.widen_builder(180);

    // A PLAIN BUILD OF THE CATALOG'S FIRST ROW, finished and loaded by nobody.
    c.press_into(builder_ref());
    c.letter(input::scan::kB, "b");
    REQUIRE(c.tool->builds.size() == 1);
    REQUIRE(c.tool->builds[0] == "skin");
    REQUIRE_FALSE(c.tool->realizes[0]);
    c.settle(bld::outcome::kSucceeded, bld::realization::kNotAsked, "zengine-skin");
    REQUIRE(c.builder_row("realize  ").find("(load-after-build loads zengine-skin now)") !=
            std::string::npos);

    // EDIT CODE CHOOSES TALLY'S RECIPE, AND THE ROW STILL NAMES THE BUILD THAT IS READY.
    c.point_at(tally_ref());
    c.choose_edit_code();
    REQUIRE(c.builder_row("recipe   ").find("tally -> zengine-example-tally") != std::string::npos);
    CHECK(c.builder_row("realize  ").find("(load-after-build loads zengine-skin now)") !=
          std::string::npos);
    c.press_into(builder_ref());

    SUBCASE("Shift+b is the button, and it spends the finished build's recipe, not the choice") {
        c.r.key(input::scan::kB, input::mod::kShift);
        REQUIRE(c.tool->builds.size() == 2);
        CHECK(c.tool->builds[1] == "skin");
        CHECK(c.tool->realizes[1]);
    }
    SUBCASE("b builds the chosen recipe, and then the button names and loads that one") {
        c.letter(input::scan::kB, "b");
        REQUIRE(c.tool->builds.size() == 2);
        CHECK(c.tool->builds[1] == "tally");
        CHECK_FALSE(c.tool->realizes[1]);
        c.settle(bld::outcome::kSucceeded, bld::realization::kNotAsked, kTallyStem);
        REQUIRE(c.builder_row("realize  ").find("(load-after-build loads zengine-example-tally now)") !=
                std::string::npos);
        c.r.key(input::scan::kB, input::mod::kShift);
        REQUIRE(c.tool->builds.size() == 3);
        CHECK(c.tool->builds[2] == "tally");
        CHECK(c.tool->realizes[2]);
    }
}

// ============================================================================
// A CMake target's editing entry: the same join, the same opening, both doors (WL-CODE-05)
// ============================================================================

TEST_CASE("Edit Code opens a CMake target recipe's editing entry through the opening office, and the Builder follows that recipe") {
    // THE SHIPPED PANES' ROUTE, ON THE EXAMPLE'S IMAGE: one recipe produces the pane's artifact,
    // and it is a CMake target that names where its code begins. Nothing about the opening, the
    // reading published or the Builder's choice differs from a single source -- the entry is the
    // file, and the build stays `--target`.
    CodeRig c("code-entry");
    c.hold({target_recipe("skin", "zengine-skin"), target_recipe("tally-tree", kTallyStem, c.source)});
    c.open();
    const std::int64_t committed_before = c.r.opening->state().committed;
    REQUIRE(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);

    c.point_at(tally_ref());
    c.choose_edit_code();

    CHECK(c.r.opening->state().committed == committed_before + 1);
    CHECK(c.r.opening->state().last_path == c.source);
    CHECK(c.editor_status().find("tally.cpp") != std::string::npos);
    CHECK_FALSE(c.r.session().notice_is_bad);
    CHECK(c.notice().find("opened the source of Tally -- recipe `tally-tree`") != std::string::npos);
    REQUIRE(c.watch->heard.size() == 1);
    CHECK(c.watch->heard[0].recipe == "tally-tree");
    CHECK(c.watch->heard[0].source == c.source);
    CHECK(c.text_of(builder_ref()).find("tally-tree -> zengine-example-tally") != std::string::npos);
    CHECK(c.tool->builds.empty());
}

TEST_CASE("the Builder's e opens a CMake target recipe's editing entry by the rule Edit Code reads, and one with none says so") {
    // BOTH DOORS READ ONE RULE (`provenance::recipe_source_of`), wired over the catalog in force
    // exactly as the host wires it: the Builder's `e` asks the project office by the recipe's
    // name, and the answer is the entry that recipe's author wrote -- or the recipe file's words.
    CodeRig c("code-entry-e");
    c.hold({target_recipe("tally-tree", kTallyStem, c.source), target_recipe("skin", "zengine-skin")});
    c.r.host.recipe_source = [&c](const std::string& id) {
        return provenance::recipe_source_of(c.r.host_recipes.all(), id);
    };
    c.open();

    c.press_into(builder_ref());
    REQUIRE(c.text_of(builder_ref()).find("tally-tree -> zengine-example-tally") != std::string::npos);
    const std::int64_t committed_before = c.r.opening->state().committed;
    c.letter(input::scan::kE, "e");
    CHECK(c.r.opening->state().committed == committed_before + 1);
    CHECK(c.r.opening->state().last_path == c.source);
    CHECK(c.editor_status().find("tally.cpp") != std::string::npos);

    // THE RECIPE THAT NAMES NO ENTRY OPENS NOTHING, AND SAYS WHICH RECIPE AND WHY.
    c.press_into(builder_ref());
    c.letter(input::scan::kC, "c");
    REQUIRE(c.text_of(builder_ref()).find("skin -> zengine-skin") != std::string::npos);
    c.letter(input::scan::kE, "e");
    CHECK(c.r.opening->state().committed == committed_before + 1);
    CHECK(c.text_of(builder_ref()).find("`skin` is a cmake_target recipe -- it names no source "
                                        "file or editing entry to open") != std::string::npos);
}

TEST_CASE("Edit Code bound to a key in command mode names no pane, says where the gesture lives, and opens nothing") {
    CodeRig c("code-keyed");
    const std::string keymap = (c.root / "keymap.json").generic_string();
    write_keymap_file(keymap, keymap_file_text("full", {{"pane.edit-code", "e"}}));
    c.r.host.keymap_path = keymap;
    c.hold({single_recipe("tally", kTallyStem, c.source)});
    c.open();
    // THE KEYS ARE COMMAND MODE'S: a press on the bare workspace hands them back.
    c.r.press_cell(0, screen_of(c.r.session()).h - 1);
    REQUIRE(c.r.session().panels.keyboard == kNoPaneKind);
    c.r.key(input::scan::kE);
    c.r.text("e");
    CHECK(c.r.session().notice_is_bad);
    CHECK(c.notice().find("edit code follows a pane you point at") != std::string::npos);
    CHECK(c.r.opening->state().last_path.empty());
    CHECK(c.watch->heard.empty());
}
