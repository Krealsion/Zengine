// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

// The Workshop panes suite — THE BUILDER, AS A LOADED WEAVE.
//
// THIS FILE OWNS the pane that used to be compiled into this host. Everything the Builder
// panel did a maker can see -- showing what the tool says, choosing a recipe, building it,
// arming and spending load-after-build, promoting and reverting an image, building the
// frontier, authoring a plan row and opening a recipe's source -- is driven here through the
// REAL `zengine-builder-pane` image, over the REAL pane protocol, against the REAL tool and
// the REAL doors this host mounts. Nothing in this file constructs the weave, reaches into
// its state, or calls one of its functions: there is a shared library on disk, a plan row
// that loads it, an office it holds, and a maker's hand.
//
// ---- WHY THE CLAIMS MOVED HERE ---------------------------------------------------
//
// They were `workshop_panels` cases (the BLD-1, BLD-2, PROJ-1, RELOAD-2 and LOAD-IT tiers)
// and they drove the same gestures against a built-in through `Session`. The pane left; the
// gestures did not. What changed is the SEAM they cross -- a keystroke is a resolved action
// id now (WL-KEY-15), a row is a `SurfaceTextRow` in a granted room rather than a painter's
// output, and the three facts the panel read off `HostContext` are asks to offices -- so the
// evidence belongs where that seam is, which is here.
//
// ⚠ AND THE CASES ARE STRONGER FOR IT, in the way the project browser's were. A built-in's
// case could assert `panels.builder.chosen`; these can only assert what a maker can see,
// which is the row on the screen and the message on the bus. Where a claim really is about
// the pane's private state, it is asked of the ROWS the pane published, because that is the
// only honest picture of it from out here.
//
// ⚠ AND ONE CLAIM IS NEW, WHICH IS THE POINT OF THE MIGRATION (VD-22): every gesture below
// is spent WHILE THE PANE HOLDS THE KEYBOARD, and the case that presses the same keys from
// anywhere else watches them reach nobody.

// main() and the framework live in doctest_main.cpp -- the shared one that
// refuses a run selecting zero cases (POP-01).
#include "workshop_support.hpp"

#include "builder-pane/vocabulary.hpp"
#include "workshop/authoring.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/pane_doors.hpp"
#include "workshop/pane_migration.hpp"

#include <algorithm>
#include <fstream>
#include <map>

namespace {

namespace pane = zengine::builder_pane;
namespace bld = zengine::builder;

/// The office and pane a saved setup names, spelled through the package's own header --
/// the durable names, not literals, so a case cannot agree with a typo.
inline PaneRef builder_ref() {
    return PaneRef{pane::kBuilderPaneRole, pane::kBuilderPane};
}

/// A STAND-IN FOR THE BUILDER TOOL, holding its office and answering as the real one does.
///
/// It is the suite's own participant and not `builder::BuilderWeave`, for the reason the
/// panels tier's `ToolSeat` was: what these cases measure is what the PANE asks for and what
/// it makes of an answer, so the answering side has to be one a case can set and count. The
/// real tool's own behaviour is `test_builder.cpp`'s, against a real compiler.
struct ToolState {
    std::int64_t answered = 0; ///< the weave's own shape; the counters a case reads are below
    ZEN_SHAPE(ToolState, 1, ZEN_FIELD(answered));
};

class Tool
    : public loom::WeaveBase<Tool, ToolState,
                             loom::Accept<bld::StatusRequested, bld::BuildRequested,
                                          bld::PromoteArtifact, bld::RevertArtifact, SeatDo>,
                             loom::Emit<bld::BuildStatus, bld::RecipeCatalog>> {
public:
    bld::RecipeCatalog catalog{};
    bld::BuildStatus next{};
    /// PLAIN MEMBERS, NOT STATE -- a case reads them directly, `ToolSeat`'s own shape.
    std::int64_t described = 0;
    std::int64_t builds = 0;
    std::vector<std::string> asked;
    std::vector<bool> realize_asked;
    std::vector<std::string> promotes;
    std::vector<std::string> reverts;

    void on(const bld::StatusRequested&, loom::Mail& mail) {
        ++described;
        ++state_.answered;
        (void)mail.publish(catalog);
        (void)mail.publish(next);
    }
    /// TAKING A BUILD PUTS THE TOOL IN A CONDITION THE BUILD HAS NOT LEFT, which is what the
    /// real one does (`outcome::kAsked`, then `kRunning`) and is load-bearing here: a status
    /// the build WILL leave is not an ending, so the pane keeps the sentence it just wrote
    /// about the ask. A stand-in that answered a settled outcome inside the ask would be
    /// answering a question nobody had waited for.
    void on(const bld::BuildRequested& ask, loom::Mail& mail) {
        ++builds;
        ++state_.answered;
        asked.push_back(ask.recipe);
        realize_asked.push_back(ask.realize);
        next.recipe = ask.recipe;
        next.realize = ask.realize;
        next.builds = builds;
        next.outcome = bld::outcome::kAsked;
        (void)mail.publish(next);
    }
    void on(const bld::PromoteArtifact& said, loom::Mail&) { promotes.push_back(said.artifact); }
    void on(const bld::RevertArtifact& said, loom::Mail&) { reverts.push_back(said.artifact); }

    /// SAY WHAT YOU ARE, unasked -- how the real tool republishes after a build settles.
    void say(loom::Mail& mail) { (void)mail.publish(next); }

    /// SPEAK INSIDE THIS WEAVE'S OWN DELIVERY -- `asker_do`'s reason exactly: `mail.publish`
    /// needs an authorship moment Loom can verify, and a case cannot manufacture one.
    std::function<void(Tool&, loom::Mail&)> next_act;
    void on(const SeatDo&, loom::Mail& mail) {
        if (next_act) {
            auto what = next_act;
            next_act = nullptr;
            what(*this, mail);
        }
    }
};

/// A LIVE WORKSHOP WITH THE REAL BUILDER PANE LOADED INTO IT.
///
/// The order is the host's, and it is the whole arrangement under test: the tool and the
/// doors are mounted BEFORE the plan runs, because the pane asks both on the very beat it is
/// first granted room -- a door mounted afterwards would be absent exactly when the only ask
/// that matters is made.
struct BuilderRig {
    TempDir dir;
    std::filesystem::path root;
    PaneRig r;
    Tool* tool = nullptr;
    std::int64_t kind = 0;
    /// The frontier this rig's host is waiting on, and the plan it holds -- the two facts
    /// the read-only door answers, settable by a case because that is what they are for.
    ProjectFrontier frontier{};
    std::vector<std::string> plan_rows;
    std::vector<PlanRowRequested> authored;
    HostContext::PlanAppend next_append{};
    HostContext::RecipeSource next_source{};
    /// ...AND ONE ANSWER PER RECIPE where a case needs the two told apart.
    std::map<std::string, HostContext::RecipeSource> sources;

    explicit BuilderRig(const char* tag) : dir(tag) {
        root = dir.path();
        r.host.project_dir = root.generic_string();
        next_append.accepted = true;
        next_append.detail = "resolved";
        // THE TOOL IS SEATED BEFORE ANYTHING ELSE, so a case can set the catalog it will
        // answer with before the pane exists to ask for it -- which is the order a real run
        // has: the tool is mounted in the host's `main` and the pane arrives by a plan row.
        mount_tool_office();
    }

    /// `with_editor` LOADS THE REAL EDITOR IMAGE BESIDE THE BUILDER, for the cases about `e`:
    /// the Editor is a weave (VD-25), so the second of the two doors `e` walks is answered by
    /// nobody unless its image is in the room.
    /// `with_manager` MOUNTS THE OPENING MANAGER BESIDE WORKSHOP, as the host does (the office
    /// `e`'s second door asks, WL-OPEN-01), and `with_project_door` the read-only project
    /// office `e`'s first door asks; each left out is a door that reaches nobody, which the
    /// refusal-at-dispatch cases are about.
    /// `with_presenter` PUTS THE SHIPPED PRESENTER IN THE PLAN, as a host's own plan row does:
    /// a menu this pane offers is granted to whoever holds `zengine.presenter`. It is a PLAN
    /// ROW rather than `load_presenter` because a rig that realized a plan has already
    /// published the plan booter's `BootState`.
    void open(std::int64_t width = 160, std::int64_t height = 48, bool with_editor = false,
              bool with_manager = true, bool with_project_door = true,
              bool with_presenter = false) {
        r.host.managed_pane = PaneRef{"zengine.editor", "editor"};
        r.mount_workshop();
        if (with_manager) {
            r.mount_opening();
        }
        mount_doors(with_project_door);
        load::LoadPlan plan;
        load::ArtifactIntent seat;
        seat.stem = pane::kBuilderPaneStem;
        seat.weave = load::WeaveIntent{pane::kBuilderPaneRole};
        plan.artifacts.push_back(seat);
        if (with_editor) {
            load::ArtifactIntent editor;
            editor.stem = "zengine-editor-pane";
            editor.weave = load::WeaveIntent{"zengine.editor"};
            plan.artifacts.push_back(editor);
        }
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
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `builder` pane");
        r.pick(builder_ref());
        kind = row()->kind;
        focus();
    }

    /// PRESS INTO THE PANE, which is the whole of what VD-22 made necessary: its rows are
    /// active only while it holds the keyboard, so every gesture below is a maker who has
    /// pointed at the Builder first. Row 0 of the room is the pane's own first row.
    void focus() {
        const ui::Rect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y);
        REQUIRE(r.session().panels.keyboard == kind);
    }

    /// A TALLER PANE, authored the way a maker's setup file authors one: some cases need the
    /// whole control strip drawn, and the strip grows with the room.
    void author_height(std::int64_t cells, std::int64_t width, std::int64_t height) {
        const Written wrote = author_pane_size(r.session().setup.active, builder_ref(), PaneSize{},
                                               PaneSize{pane_unit::kSubcells, subs(cells)});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        r.extent(width, height);
    }

    /// Hand the keys back the way a maker does -- a press on the bare workspace.
    void unfocus() {
        r.press_cell(0, screen_of(r.session()).h - 1);
        REQUIRE(r.session().panels.keyboard != kind);
    }

    const RuntimePane* row() {
        return r.session().panels.runtime.find(pane::kBuilderPaneRole, pane::kBuilderPane);
    }

    /// A KEY AND ITS CHARACTER QUEUED AND NOT DRAINED, so a case can place a real message at
    /// an exact interval of the conversation `e` begins; the case pumps.
    void enqueue_letter(std::int64_t scancode, const char* typed) {
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::KeyPressed{scancode, "", input::mod::kNone}), loom::WeaveId{},
            loom::WeaveId{}, 0));
        (void)r.bus.publish(loom::Message(loom::to_value(input::TextEntered{typed}),
                                          loom::WeaveId{}, loom::WeaveId{}, 0));
    }

    /// A STRANGER THAT CAN SAY `zen.DispatchRefused` AS A SHAPE, granted so the forgery case
    /// proves the pane's refusal to act on ordinary speech and not the bus's grant refusal.
    DoorAsker* stranger = nullptr;
    loom::WeaveId stranger_id{};
    void mount_stranger() {
        auto held = std::make_unique<DoorAsker>(std::string(kDoorAskerOffice));
        stranger = held.get();
        loom::Grant grant;
        grant.allow_to_any(loom::DispatchRefused::zen_name, loom::DispatchRefused::zen_version);
        stranger_id = r.bus.register_weave(std::move(held), std::move(grant),
                                           std::string(kDoorAskerOffice));
        stranger->zen_set_self(stranger_id);
        stranger->id = stranger_id;
    }
    void forge_refusal(loom::WeaveId to, std::uint64_t attempt, const char* role,
                       const char* shape) {
        REQUIRE(stranger != nullptr);
        stranger->next = [to, attempt, role, shape](DoorAsker&, loom::Mail& mail) {
            loom::DispatchRefused forged;
            forged.attempt = std::to_string(attempt);
            forged.role = role;
            forged.shape = shape;
            forged.version = 1;
            forged.reason = "NoSuchTarget";
            (void)mail.as_role(kDoorAskerOffice).send(to, forged);
        };
        (void)r.bus.send(stranger_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                    loom::WeaveId{}, 0));
    }

    /// The Editor pane's handle, when its image was loaded beside the Builder.
    std::int64_t editor_kind() {
        const RuntimePane* editor = r.session().panels.runtime.find("zengine.editor", "editor");
        REQUIRE(editor != nullptr);
        return editor->kind;
    }
    std::string editor_status() {
        const std::vector<std::string> rows = pane_rows(r, editor_kind());
        REQUIRE_FALSE(rows.empty());
        return rows[0];
    }

    std::vector<std::string> shown() { return pane_rows(r, kind); }

    /// Every row the pane published, joined -- what a maker reads at the pane's rectangle.
    std::string text() {
        std::string all;
        for (const std::string& row_text : shown()) {
            all += row_text;
            all += '\n';
        }
        return all;
    }

    /// A bare-letter gesture, as a backend really reports one: the key transition AND the
    /// character it produced.
    void letter(std::int64_t scancode, const char* typed) {
        r.key(scancode);
        r.text(typed);
    }

    void tool_says() {
        // The tool republishing on its own beat -- a build settling, a realization answered.
        seat_do([](Tool& t, loom::Mail& mail) { t.say(mail); });
    }

    /// A NEW ROOM AND NOTHING ELSE: the surface changes size, Workshop grants the pane its room
    /// again, and the pane says its rows -- the ordinary repaint that exposes a notice cleared in
    /// private. Required to be a real grant, so a deduplicated extent cannot pass for one.
    void regrant() {
        const ExternalPane* seat = r.session().panels.external_pane(kind);
        REQUIRE(seat != nullptr);
        const std::int64_t rows = seat->rows;
        const std::int64_t columns = seat->columns;
        wide_ = !wide_;
        r.extent(wide_ ? 160 : 150, wide_ ? 48 : 44);
        const ExternalPane* after = r.session().panels.external_pane(kind);
        REQUIRE(after != nullptr);
        REQUIRE_MESSAGE((after->rows != rows || after->columns != columns),
                        "the surface changed and the pane's room did not");
    }
    bool wide_ = true;

    /// A KEY QUEUED AND NOT DRAINED, so two keys land in one poll; `settle` drains.
    void enqueue_key(std::int64_t scancode) {
        (void)r.bus.publish(loom::Message(
            loom::to_value(input::KeyPressed{scancode, "", input::mod::kNone}), loom::WeaveId{},
            loom::WeaveId{}, 0));
    }
    void settle() { r.bus.drain_until_idle(); }

    /// The ids the pane declares right now, in its own order.
    std::vector<std::string> declared() {
        std::vector<std::string> ids;
        const RuntimePane* seat = row();
        REQUIRE(seat != nullptr);
        for (const v2::PaneActionRow& a : seat->actions) {
            ids.push_back(a.id);
        }
        return ids;
    }

    void seat_do(std::function<void(Tool&, loom::Mail&)> what) {
        tool->next_act = std::move(what);
        (void)r.bus.send(tool_id, loom::Message(loom::to_value(SeatDo{}), loom::WeaveId{},
                                                loom::WeaveId{}, 0));
        r.bus.drain_until_idle();
    }

    loom::WeaveId tool_id{};

    void mount_tool_office() {
        auto held = std::make_unique<Tool>();
        Tool* raw = held.get();
        loom::Grant say;
        say.allow_to_any(bld::BuildStatus::zen_name, bld::BuildStatus::zen_version);
        say.allow_to_any(bld::RecipeCatalog::zen_name, bld::RecipeCatalog::zen_version);
        tool_id = r.bus.register_weave(std::move(held), std::move(say),
                                       std::string(bld::kBuilderRole));
        raw->zen_set_self(tool_id);
        tool = raw;
    }

    /// The read-only project office `mount_doors` seated, when it seated one.
    loom::WeaveId project_id{};

    void mount_doors(bool with_project_door = true) {
        r.host.frontier = [this] { return frontier; };
        r.host.plan_names = [this](const std::string& stem) {
            for (const std::string& held : plan_rows) {
                if (held == stem) {
                    return true;
                }
            }
            return false;
        };
        r.host.append_plan_row = [this](const std::string& stem, const std::string& role,
                                        const std::string& recipe) {
            authored.push_back(PlanRowRequested{stem, role, recipe});
            if (next_append.accepted) {
                plan_rows.push_back(stem);
            }
            return next_append;
        };
        // ONE ANSWER FOR EVERY RECIPE, unless a case says a different one PER recipe -- which is
        // what a case proving WHICH source was opened needs, rather than one that cannot tell
        // two recipes' sources apart.
        r.host.recipe_source = [this](const std::string& recipe) {
            const auto named = sources.find(recipe);
            return named == sources.end() ? next_source : named->second;
        };
        if (!with_project_door) {
            return;
        }

        auto project = std::make_unique<ProjectDoor>(r.host.project_dir, marks_, r.host.frontier,
                                                     r.host.plan_names, r.host.recipe_source);
        ProjectDoor* praw = project.get();
        loom::Grant say_project;
        say_project.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        say_project.allow_to_any(ProjectFrontierSaid::zen_name, ProjectFrontierSaid::zen_version);
        say_project.allow_to_any(PlanNames::zen_name, PlanNames::zen_version);
        say_project.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
        const loom::WeaveId pid = r.bus.register_weave(std::move(project), std::move(say_project),
                                                       std::string(kProjectRole));
        praw->zen_set_self(pid);
        project_id = pid;

        auto plan = std::make_unique<PlanDoor>(r.host.append_plan_row);
        PlanDoor* draw = plan.get();
        loom::Grant say_plan;
        say_plan.allow_to_any(PlanRowWritten::zen_name, PlanRowWritten::zen_version);
        const loom::WeaveId did =
            r.bus.register_weave(std::move(plan), std::move(say_plan), std::string(kPlanRole));
        draw->zen_set_self(did);
    }

private:
    std::string marks_;
};

/// The nine ids the pane declares while it is browsing, and the ones its role line does --
/// spelled through the package's own header so a case cannot agree with a typo.
const std::vector<std::string> kBrowsingIds = {
    pane::kActionBuild,      pane::kActionBuildRealize, pane::kActionPromote,
    pane::kActionRevert,     pane::kActionLoadIt,       pane::kActionRecipeNext,
    pane::kActionRecipeBack, pane::kActionFrontier,     pane::kActionEditSource};

/// ...and how many the pane declares while browsing: those nine, the output reader's own, and
/// the five the mouse work added -- the recipe list, this pane's own menu, and the two halves
/// of load-after-build, which carry no default key and exist so a control and a menu row can
/// each name one operation.
const std::vector<std::string> kAddedBrowsingIds = {pane::kActionOutput, pane::kActionRecipes,
                                                    pane::kActionMenu, pane::kActionArm,
                                                    pane::kActionLoadBuilt};
const std::size_t kBrowsingRows = kBrowsingIds.size() + kAddedBrowsingIds.size();

/// The rows the role line declares: its commit, this pane's menu and its cancel.
const std::vector<std::string> kRoleLineIds = {pane::kActionCommit, pane::kActionMenu,
                                               pane::kActionCancel};

inline bld::RecipeCatalog catalog_of(std::vector<std::pair<std::string, std::string>> rows,
                                     std::string source = "/project/recipes.json") {
    bld::RecipeCatalog out;
    for (const auto& row : rows) {
        out.recipes.push_back(bld::RecipeSummary{row.first, row.second});
    }
    out.source = std::move(source);
    return out;
}

} // namespace

// ============================================================================
// BLD-WEAVE — the pane arrives, and it is a stranger
// ============================================================================

TEST_CASE("BLD-WEAVE: the Builder arrives by a plan row, under an office of its own") {
    // ⭐ THE PHASE'S CENTRAL CLAIM, MEASURED AT THE SEAM. Workshop compiled nothing for this
    // pane, minted no kind for it and holds no branch on it: what puts it on a maker's
    // screen is a row in an editable file naming an artifact, and an offer this host learns
    // about at runtime like any other.
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
    // ...AND THE HOST'S OWN CATALOG OFFERS NO SUCH PANE ANY MORE.
    for (const PanelKind& row : kPanelCatalog) {
        CHECK(std::string(row.pane) != std::string(pane::kBuilderPane));
        CHECK(std::string(row.name) != std::string(pane::kBuilderPaneName));
    }
}

TEST_CASE("BLD-WEAVE: the pane asks the tool what it is on its own room grant, and shows it") {
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

TEST_CASE("BLD-WEAVE: the pane declares the nine ids a maker's keymap file already names") {
    // ⭐ THE PROMISE THIS MIGRATION WAS MADE TO KEEP. `builder.build` and its eight
    // neighbours were WORKSHOP action ids; they are the PANE's now, spelled exactly as they
    // were, so an authored override that moved `builder.build` keeps moving it.
    BuilderRig b("bld-rows");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    const RuntimePane* seat = b.row();
    REQUIRE(seat != nullptr);
    std::vector<std::string> declared;
    for (const v2::PaneActionRow& row : seat->actions) {
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

TEST_CASE("BLD-WEAVE: `b` builds only after the maker has pressed into the pane") {
    // ⭐ VD-22, AS A CASE. The nine rows were command-mode rows: `b` built from anywhere in
    // Workshop as long as a Builder panel happened to be open. They are the pane's now, so
    // the same key from anywhere else reaches nobody -- and nothing was put in its place.
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

TEST_CASE("BLD-WEAVE: the choice moves with `c`, wraps, and asks for nothing") {
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
    // NOTHING WAS ASKED OF THE TOOL: choosing is a maker's act on a presentation.
    CHECK(b.tool->asked.empty());
}

TEST_CASE("BLD-WEAVE: `b` builds the recipe the maker chose, by name") {
    BuilderRig b("bld-chosen");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();

    b.letter(input::scan::kC, "c");
    b.letter(input::scan::kB, "b");
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "two");
}

TEST_CASE("BLD-WEAVE: PROJ-1 -- the choice follows its RECIPE to a new row, not its index") {
    // ⭐ WL-PROJ-07 THROUGH THE SEAM. The pane holds the chosen recipe's NAME, so a catalog
    // that comes back reordered moves the choice with no work at all -- and there is no
    // index to carry wrongly, which is the whole reason the state field is a string.
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

TEST_CASE("BLD-WEAVE: PROJ-1 -- a choice whose recipe is gone is released, not inherited") {
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
    // project waits on, `f` chooses it, and a second producer arrives: the pick the maker made
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

TEST_CASE("BLD-WEAVE: an empty catalog is said plainly, and `b` asks for nothing") {
    BuilderRig b("bld-empty");
    b.tool->catalog = catalog_of({});
    b.open();

    CHECK(b.text().find("this project has no build recipes") != std::string::npos);
    b.letter(input::scan::kB, "b");
    CHECK(b.tool->asked.empty());
    CHECK(b.text().find("nothing was asked for") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — P-WORK-20: the pane says which catalog is in force again
// ============================================================================

TEST_CASE("BLD-WEAVE: P-WORK-20 -- the pane names the catalog in force, from RecipeCatalog v2") {
    // ⭐ THE ONE MAKER-FACING LOSS PR #16 LEFT, PAID BACK. The row that named the authored
    // catalog read a session projection the project browser wrote; the browser became a
    // weave and the projection left with it, so the row was retired with the loss written
    // into WL-PROJ-09. `RecipeCatalog` v2 carries `source` from the one owner that holds it
    // beside the rows, and this pane is its consumer.
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
// BLD-WEAVE — RELOAD-2: one action in two states, and the two acts a reload leaves
// ============================================================================

TEST_CASE("BLD-WEAVE: RELOAD-2 -- `B` before a build is the toggle, and `b` reads it") {
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

TEST_CASE("BLD-WEAVE: RELOAD-2 -- after a plain build that worked, `B` is the button") {
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

TEST_CASE("BLD-WEAVE: RELOAD-2 -- `P` and `R` are one offer each, about the built artifact") {
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
    // ...and the revert's sentence says what it leaves alone: the source the maker saved.
    CHECK(b.text().find("saved source unchanged") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — BLD-2: the frontier, asked for rather than read
// ============================================================================

TEST_CASE("BLD-WEAVE: BLD-2 -- the frontier row comes from the host's read-only door") {
    // ⭐ THE FACT THE PANEL READ ALIVE OFF `HostContext` CROSSES AS A SENTENCE NOW. The
    // realization owner is in the host's `main` and the pane is a loaded image, so what it
    // has is a picture it asked for -- and knowing what a project waits on is not permission
    // to perform a row.
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

TEST_CASE("BLD-WEAVE: BLD-2 -- `f` builds and realizes the one recipe that makes the frontier") {
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

TEST_CASE("BLD-WEAVE: BLD-2 -- `f` refuses in words, and never chooses between recipes") {
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

    // SEVERAL PRODUCE IT: named and counted, and the pick left to the maker.
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

TEST_CASE("BLD-WEAVE: BLD-2 -- the recipe `f` took as the one producer carries no pick of another recipe") {
    BuilderRig b("bld-f-took");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.frontier.waiting = true;
    b.frontier.artifact = "a";
    b.open();

    b.letter(input::scan::kC, "c"); // an explicit pick: `two`, which does not produce `a`
    REQUIRE(b.text().find("two -> b  (2/2)") != std::string::npos);
    // ONE PRODUCER: `f` chooses it, visibly, and builds it -- the gesture's own choice, not the maker's.
    b.letter(input::scan::kF, "f");
    REQUIRE(b.tool->asked.size() == 1);
    REQUIRE(b.tool->asked[0] == "one");
    REQUIRE(b.text().find("one -> a  (1/2)") != std::string::npos);

    // A SECOND PRODUCER ARRIVES: the pick still names `two`, so nothing picked stands between them.
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "a"}});
    b.seat_do([](Tool& t, loom::Mail& mail) { (void)mail.publish(t.catalog); });
    b.letter(input::scan::kF, "f");
    CHECK(b.tool->asked.size() == 1);
    CHECK(b.text().find("2 recipes produce `a` (`one`, `three`)") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — LOAD-IT: the plan row, authored through the acting door
// ============================================================================

TEST_CASE("BLD-WEAVE: LOAD-IT -- `o` asks for a role in the pane's own room, and authors it") {
    // ⭐ THE HOST MODAL BECAME A LINE INSIDE THE PANE (the Files pattern). The built-in
    // opened Workshop's `AuthoringPrompt` in a keyboard context of Workshop's own; a weave
    // has one room and no context of Workshop's, so the line lives in its own rows and its
    // two gestures are two of its own declared actions.
    BuilderRig b("bld-load");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    b.letter(input::scan::kO, "o");
    CHECK(b.text().find("role for zengine-snake") != std::string::npos);
    // ...AND WHILE THE LINE HAS THE KEYBOARD THE PANE DECLARES ITS THREE MODE ROWS AND NO
    // MORE, so every other key reaches it as an ordinary keystroke for the line to consume
    // (WL-FILES-16, one pane over). None of the build verbs is among them.
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

TEST_CASE("BLD-WEAVE: LOAD-IT -- Escape abandons the line, and nothing is written") {
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

TEST_CASE("BLD-WEAVE: LOAD-IT -- an artifact the plan already names is refused before the line") {
    // THE READ AND THE ACT ARE TWO OFFICES, and this is the read: the pane asks
    // `zengine.project` whether the plan names the stem and opens the line only if it does
    // not, so a maker never types a role for a row that cannot be written.
    BuilderRig b("bld-load-dup");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.plan_rows.push_back("zengine-snake");
    b.open();

    b.letter(input::scan::kO, "o");
    CHECK(b.text().find("already in this project's plan") != std::string::npos);
    CHECK(b.text().find("role for") == std::string::npos);
    CHECK(b.authored.empty());
}

TEST_CASE("BLD-WEAVE: LOAD-IT -- a refused row is said in the owner's own words") {
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

TEST_CASE("BLD-WEAVE: LOAD-IT -- a row whose product is built finishes with the button's act") {
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
    // ...AND THE MAKER IS TOLD, in the sentence a narrow pane can actually carry: the pane's
    // room is one stack slot wide, so its notices front-load their meaning the way a path
    // does not (WL-PROJ-10's rule, one artifact over) -- the act first, the details after.
    CHECK(b.text().find("loading `zengine-snake` now") != std::string::npos);
}

// ============================================================================
// BLD-WEAVE — the Editor door, reached by a recipe's name
// ============================================================================

TEST_CASE("BLD-WEAVE: `e` opens the chosen recipe's source, resolved by the host") {
    // ⭐ THE PANE NAMES A RECIPE AND NEVER A PATH. `RecipeSummary` is `{recipe, artifact}` on
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
    REQUIRE(b.r.session().panels.has(editor));
    CHECK(b.r.session().panels.keyboard == editor);
    CHECK(b.editor_status().rfind("saved L1:C1/2", 0) == 0);
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    const std::vector<std::string> rows = pane_rows(b.r, editor);
    CHECK(std::find(rows.begin(), rows.end(), "int main() {}") != rows.end());
}

TEST_CASE("BLD-WEAVE: a refusal from either door is said in the pane's own row") {
    // THE FIRST DOOR'S REFUSAL: a kind with no single source, in the recipe file's own words.
    BuilderRig b("bld-edit-refuse");
    b.tool->catalog = catalog_of({{"block", "zen-block"}});
    b.next_source.known = true;
    b.next_source.kind = "cmake_target"; // no single source to edit
    b.open(160, 48, /*with_editor=*/true);

    b.letter(input::scan::kE, "e");
    CHECK_FALSE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.text().find("names no source file or editing entry") != std::string::npos);

    // THE SECOND DOOR'S REFUSAL: a file that is not there, in the Editor's own words.
    BuilderRig c("bld-edit-missing");
    c.tool->catalog = catalog_of({{"gone", "zen-gone"}});
    c.next_source.known = true;
    c.next_source.kind = "single_source";
    c.next_source.source = (c.root / "absent.cpp").generic_string();
    c.open(160, 48, /*with_editor=*/true);
    c.letter(input::scan::kE, "e");
    CHECK_FALSE(c.r.session().panels.has(c.editor_kind()));
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

TEST_CASE("BLD-WEAVE: closing the pane forgets its copy; the TOOL keeps its own count") {
    // The law this is about is WL-PROJ-12. The pane's copy of somebody else's facts is a member and
    // not state, and Workshop stops granting it a room when the pane is removed -- so
    // reopening asks again and is answered with the tool's own running total.
    BuilderRig b("bld-forget");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();
    b.letter(input::scan::kB, "b");
    const std::int64_t asked_once = b.tool->described;

    b.unfocus();
    b.r.pick(builder_ref()); // the same door removes it
    REQUIRE_FALSE(b.r.session().panels.has(b.kind));
    b.r.pick(builder_ref());
    b.focus();
    CHECK(b.tool->described > asked_once);
    // ...AND THE TOOL'S OWN COUNTER IS WHAT COMES BACK. The pane held no copy across the
    // close -- Workshop stopped granting it a room, and its picture is a member and not
    // state -- so what it shows now is the tool's own running total, which a pane that owned
    // the state could not have produced. (`asks N ever` is the row that carries the number,
    // and it is the FIRST fact this pane's composition gives up under a constrained budget,
    // so the count is read from the tool rather than from a row that may not have a seat.)
    CHECK(b.tool->builds == 1);
    CHECK(b.text().find("recipe   snake") != std::string::npos);
}

TEST_CASE("BLD-WEAVE: PANE-MIG -- the office the migration writes is the one this pane holds") {
    // THE TWO SPELLINGS, CHECKED AGAINST EACH OTHER. `pane_migration.hpp` names the new
    // office as a literal because the host does not link this weave; this is the seam where
    // a divergence between the two would actually be caught.
    CHECK(std::string(pane_migration::kBuilderProvider) == std::string(pane::kBuilderPaneRole));
    CHECK(std::string(pane_migration::kBuilderPane) == std::string(pane::kBuilderPane));
    // ...AND A SAVED SETUP NAMING THE BUILT-IN RESOLVES TO THIS PANE AFTER THE CONVERSION.
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
    CHECK(resolve_pane(s.panes[0].ref, b.r.session().panels).value_or(kNoPaneKind) == b.kind);
}

TEST_CASE("BLD-WEAVE: the package links no kernel and mounts nothing") {
    // ⭐ THE BUILD GRAPH, READ AS TEXT. This pane asks offices and hears answers; it never
    // touches the control door or the Manager, and the five names a presentation may not
    // spell are as forbidden in its own image as they are in this host's sources.
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

TEST_CASE("BLD-WEAVE: a maker's authored override for a retired Workshop id keeps working") {
    // ⭐ THE PROMISE THE MIGRATION WAS MADE TO KEEP, END TO END. `builder.build` was a WORKSHOP
    // action id and is the PANE's now; `authoring.commit` was a row of a Workshop keyboard
    // context and is one of the pane's two mode rows. A maker who moved either -- months ago,
    // against a build where both were this host's -- opens this one and finds the key where
    // they put it. Nothing was renamed, so nothing had to be told.
    //
    // THE FILE IS READ BEFORE THE PANE EXISTS, which is the order that matters: both rows are
    // preserved as unknown at load (WL-KEY-06), and the pane's declaration is what makes them
    // known -- at which point the maker's own gesture is applied to it (WL-KEY-15).
    TempDir keys("bld-override");
    const std::string path = keys.file("keymap.json");
    write_keymap_file(path, keymap_file_text("default", {{"builder.build", "ctrl+u"},
                                                         {"authoring.commit", "ctrl+j"}}));

    BuilderRig b("bld-override");
    b.r.host.keymap_path = path;
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    b.open();

    // THE MOVED GESTURE BUILDS, and the default no longer does -- which is what "moved" means.
    b.r.key(input::scan::kU, input::mod::kCtrl);
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked[0] == "snake");
    b.letter(input::scan::kB, "b");
    CHECK(b.tool->asked.size() == 1); // `b` is an ordinary character here now

    // ...AND SO DOES THE ROLE LINE'S COMMIT, under the id the host used to declare it with.
    b.letter(input::scan::kO, "o");
    b.r.text("zengine.oven");
    b.r.key(input::scan::kReturn); // the default, which the maker moved away from
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
// BLD-WEAVE -- a refusal at enqueue, a refusal at dispatch, and a forgery are three facts (WL-OPEN-07)
// ============================================================================

TEST_CASE("BLD-WEAVE: a lookup queued to a project office nobody holds and an open queued to an opening office nobody holds are each refused at dispatch by that attempt, in words, and a fresh e takes once each office is present") {
    // THE FIRST DOOR REACHES NOBODY: no project office is held. The lookup's shape still
    // resolves -- this pane DECLARES `RecipeSourceRequested` in its `Emit<...>`, and since Loom's
    // ABI v9 a declared shape is registered by its emitter at load, for as long as it lives --
    // so the lookup is queued to the unheld project office and refused at dispatch, and Loom's
    // own notice names the attempt; the pane handles it in words naming the lookup as the stage
    // that failed. (Not silence, and never a fabricated stage. The pane's other branch -- a
    // ticket that is not valid because nothing was queued -- is no longer reachable through a
    // shape this pane declares, and stays source-traced: `edit_source` in builder-pane/pane.cpp.)
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
    CHECK_FALSE(a.r.session().panels.has(a.editor_kind()));
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
    CHECK_FALSE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.r.session().panels.keyboard == b.kind);
    // ...AND A FRESH `e` TAKES once the office is held.
    b.r.mount_opening();
    b.letter(input::scan::kE, "e");
    REQUIRE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.r.session().panels.keyboard == b.editor_kind());
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
}

TEST_CASE("BLD-WEAVE: a lookup queued to the project office and refused at dispatch -- the office gone before delivery -- is said by that exact attempt at the lookup stage, opens nothing, and a fresh e takes once the office is back") {
    // ⭐ LOOM'S LATER WORD, NOT THE IMMEDIATE ONE. The office is held and the lookup's shape
    // is declared when `e` asks, so the lookup is QUEUED with a valid ticket. The office is then
    // killed -- a real lifecycle change, made after the turn that queued the lookup and before
    // the turn that would deliver it -- so Loom refuses that queued attempt at dispatch and
    // tells the pane by it (`zen.DispatchRefused`). The immediate enqueue refusal is the case
    // above and a stranger's forged notice the case below; none of the three proves another.
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
    // THE MAKER READS THE RECIPE, THE STAGE, THE OFFICE AND THE REASON -- not the enqueue
    // refusal's words -- and nothing was opened.
    CHECK_MESSAGE(b.text().find("`snake`: the source could not be looked up -- it could not "
                                "reach zengine.project (TargetUnavailable)") != std::string::npos,
                  b.text());
    CHECK(b.text().find("nothing was queued") == std::string::npos);
    CHECK(opens_asked == 0);
    CHECK_FALSE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.r.opening->state().op == 0);
    CHECK(b.r.opening->state().committed == 0);
    CHECK(bus.resolve_schema(RecipeSourceRequested::zen_name,
                             RecipeSourceRequested::zen_version) != nullptr);
    // THE OFFICE COMES BACK -- revived in place, the same office -- and a fresh `e` takes.
    const std::string bytes = bus.snapshot_bytes(b.project_id);
    REQUIRE(bus.swap_state(b.project_id, bytes).revived);
    b.letter(input::scan::kE, "e");
    REQUIRE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.r.session().panels.keyboard == b.editor_kind());
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    // ...AND THE REFUSAL IT SPENT IS GONE FROM THE PANE'S PUBLISHED ROWS -- the rows Workshop holds
    // for it, which are the rows a maker reads beside the opened source.
    CHECK_MESSAGE(b.text().find("could not be looked up") == std::string::npos, b.text());
}

TEST_CASE("the Builder's refusal leaves its published rows at the maker's next e while that lookup is still unanswered, stays gone once the source opens, and a new refusal stands through a repaint until the act after it") {
    // A PRIVATE STATE CHANGE IS COMPLETE WHEN THE PUBLISHED PICTURE SAYS IT. The pane clears its
    // notice where the maker acts; an act whose answer is still on its way publishes nothing of
    // its own, so the rows Workshop holds must be said again at that act -- not at the answer,
    // which may be a while, and never inside `say`, which would lose a new refusal on the first
    // unrelated repaint. Every row checked here is the pane's PUBLISHED row, as Workshop admitted it.
    BuilderRig b("bld-notice-spent");
    b.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    const std::string src = (b.root / "snake.cpp").generic_string();
    put_source(b.root / "snake.cpp", "int main() {}\n");
    b.open(160, 48, /*with_editor=*/true, /*with_manager=*/true, /*with_project_door=*/false);
    // A STANDING REFUSAL: `e` with no project office, so nothing could be queued.
    b.letter(input::scan::kE, "e");
    REQUIRE_MESSAGE(b.text().find("could not be looked up") != std::string::npos, b.text());
    // ...AND A REPAINT THE MAKER DID NOT MAKE KEEPS IT: the tool republishes, the pane says again.
    b.tool_says();
    CHECK_MESSAGE(b.text().find("could not be looked up") != std::string::npos, b.text());
    // THE OFFICE ARRIVES HOLDING ITS ANSWER, and the maker presses `e` again.
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
    CHECK_FALSE(b.r.session().panels.has(b.editor_kind()));
    CHECK_MESSAGE(b.text().find("could not be looked up") == std::string::npos, b.text());
    // THE ANSWER, released: the source opens, and the rows stay clean.
    release();
    REQUIRE(b.r.session().panels.has(b.editor_kind()));
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
    // ...until the maker's next act, which says its own.
    b.letter(input::scan::kC, "c");
    CHECK_MESSAGE(b.text().find("`snake` names no file here") == std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("build recipe: snake") != std::string::npos, b.text());
}

TEST_CASE("a key the Builder's role line does not take is no act: the notice stands through a repaint, and a key it takes spends it") {
    // THE OTHER HALF OF "SPENT MEANS PUBLISHED": what is not an act spends nothing. The line
    // consumes its editing keys and refuses the rest; a refused key used to clear the notice
    // privately and say no rows, so the next unrelated repaint dropped a sentence the maker
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
    // WHAT IS NOT AN ACT SPENDS NOTHING, FOR AN ID AS FOR A KEY. The pane cleared its notice
    // before it asked what the id meant in the mode it was in, then said its rows without it --
    // so an id nobody declared, or one that raced the pane's own re-declaration, erased the
    // line's instructions or the cancel's answer and did nothing else.
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

TEST_CASE("BLD-WEAVE: a forged refusal naming the pane's own live attempt settles nothing at either stage, and the open completes") {
    // THE PROVENANCE IS THE FACT. While each ask is genuinely outstanding, a stranger says
    // `zen.DispatchRefused` with the RIGHT attempt number -- read off the bus's tap when the
    // ask was delivered -- and the pane settles nothing on it: the conversation goes on to
    // its real answer, and the source opens.
    //
    // ⚠ OUTSTANDING MEANS NOT YET ANSWERED. The real project door answers inside the delivery
    // that asked, so a forgery queued once the tap has seen the lookup is delivered behind the
    // answer and protects nothing. The lookup here is answered by an office that holds its
    // answer until the forgery has reached the pane, and the order the pane heard things in is
    // asserted, so a forgery that stops arriving in time turns this case red.
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
    REQUIRE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.editor_status().find("snake.cpp") != std::string::npos);
    CHECK(b.text().find("could not reach") == std::string::npos);
    CHECK(b.text().find("could not be looked up") == std::string::npos);
}

TEST_CASE("BLD-WEAVE: the Editor's refusal of a source reaches a narrow Builder row reason first, so the row's cut takes the tail and not why") {
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
    REQUIRE(b.r.session().panels.keyboard == b.editor_kind());
    b.r.text("x");
    REQUIRE(b.editor_status().find("UNSAVED") != std::string::npos);
    // THE SECOND `e`, BACK IN THE BUILDER, ASKS FOR ANOTHER SOURCE, AND THE EDITOR REFUSES IT.
    b.focus();
    b.next_source.source = (b.root / "other.cpp").generic_string();
    b.letter(input::scan::kE, "e");
    const std::string sentence = "the Editor holds unsaved changes to " + snake +
                                 " -- save source or discard source edits in the Editor first; "
                                 "nothing was opened";
    const ExternalPane* seat = b.r.session().panels.external_pane(b.kind);
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
//
// WHAT THESE CASES ARE FOR. The Builder accepted no press at all; it has a strip of labelled
// controls, a list of the catalog, a menu of its own and a picture fence now. The risks are a
// control that acts on the wrong subject -- the three whose subject is NOT the maker's choice
// are exactly where that goes wrong -- a mode a hand cannot leave, and a press aimed at rows
// the pane has replaced.

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
/// make the ready state true (`BLD-WEAVE: RELOAD-2`, above).
void bp_settled(BuilderRig& b, const char* recipe, const char* artifact, std::int64_t outcome) {
    b.tool->next.recipe = recipe;
    b.tool->next.artifact = artifact;
    b.tool->next.outcome = outcome;
    b.tool->next.realization = bld::realization::kNotAsked;
    b.tool_says();
}

} // namespace

TEST_CASE("BLD-MOUSE: the Builder's controls perform the operations its keys perform") {
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

TEST_CASE("BLD-MOUSE: the recipe list chooses by hand, and looking is not choosing") {
    // (*) THE INTELLIGIBLE VISIBLE ROUTE TO A CHOICE. A maker sees the catalog on rows, moves
    // inside it, and takes one -- and the cursor of the list is NOT the choice until they do,
    // so merely looking at a recipe cannot arm the next build against it.
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
        // THE FOCUS RULE, ONE MODE OVER. A maker coming back to this pane presses into it, and
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

TEST_CASE("BLD-MOUSE: the control that loads what was built names the BUILT recipe, not the choice") {
    // (*) THE SUBJECT THAT IS NOT THE MAKER'S CHOICE. `builder.build-realize` sends the
    // FINISHED build's own recipe again with the second intention aboard, and the choice may
    // have moved since. A control reading `load built a` must load `a`, whatever row the maker
    // has picked out -- and a maker who reads that face must not be arming the next build.
    //
    // (X) MUTATIONS, MEASURED. `load_built` sending `known_.recipes[cursor_row()].recipe`: the
    //   build goes to `two` and this case says so. `load_built` without its `ready_to_load`
    //   guard, refusing nothing: the arm-only branch is never reached and the second half of
    //   this case -- arming while an artifact stands built -- stops being refused.
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

TEST_CASE("BLD-MOUSE: while an artifact stands built, arming the next build is a different answer and says so") {
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

TEST_CASE("BLD-MOUSE: a right press offers the Builder's own rows, and a menu from another mode acts on nothing") {
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
        // EVERY ROW THAT CAN NAME ITS SUBJECT DOES. The three that act on the maker's CHOICE
        // name it too now, so a menu standing open across a catalog that moved is judged
        // against what it promised rather than against whatever the choice became.
        CHECK(any_row(offered, "build `one`"));
        CHECK(any_row(offered, "turn load-after-build on"));
        CHECK(any_row(offered, "add `one`'s artifact to the load plan..."));
        CHECK(any_row(offered, "edit `one`'s source"));
        CHECK(any_row(offered, "manage this pane..."));
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

TEST_CASE("BLD-MOUSE: a press that names a picture the Builder has replaced is refused in words") {
    // THE FENCE, MEASURED DIRECTLY. The Builder's rows move whenever what it was told changes,
    // so a press queued against an older picture must be refused rather than resolved against
    // the rows that arrived. The picture number is the pane's own, so a case has to say one --
    // which the office Workshop holds can.
    PaneRig r;
    PaneWatcher* watch = r.mount_watcher();
    REQUIRE(r.load(pane::kBuilderPaneStem, WORKSHOP_SO_BUILDER_PANE, pane::kBuilderPaneRole)
                .valid());
    r.drive_watcher(watch, [](PaneWatcher& wv, loom::Mail& m) {
        wv.grant(m, pane::kBuilderPaneRole, PaneRoom{pane::kBuilderPane, 10, 70});
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
    const std::size_t said = watch->content.size();

    r.drive_watcher(watch, [control, column, now](PaneWatcher& wv, loom::Mail& m) {
        wv.press(m, pane::kBuilderPaneRole,
                 v3::PanePressed{pane::kBuilderPane, control, column, true, now + 7});
    });
    REQUIRE(watch->content.size() > said);
    CHECK(watch->content.back().rows[0].text.find("the rows moved -- press again") !=
          std::string::npos);
}

// =============================================================================
// The review's reproductions, repaired — and the paths beside them
// =============================================================================
//
// WHAT THESE CASES ARE FOR. An independent review drove this pane through the real weave and
// found four behavioural gaps in it: a named operation that acted on whatever had arrived while
// the maker was reading it, a menu shortcut that ate ordinary text, a menu row the dispatcher
// answered to with silence, and a mode whose menu did not carry its own controls. Each case
// below is that review's own reproduction, kept at the assertion it failed on, with the
// adjacent paths the repair had to keep working.

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

/// WALK THE PRESENTER'S CURSOR TO THE ROW READING `row` AND TAKE IT, as a maker with the keys
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
std::string bp_last_asked(BuilderRig& b) {
    return b.tool->asked.empty() ? std::string("nothing") : b.tool->asked.back();
}

} // namespace

TEST_CASE("BLD-MOUSE: an open menu row naming an artifact loads THAT artifact or refuses") {
    // ⭐ THE REVIEW'S FIRST FINDING (B1), REPRODUCED AND REPAIRED. A menu stands open across any
    // number of the maker's other acts and across every build that settles under it. Its subject
    // was the constant `builder`, so a row reading `load the built `a` now` was still a row of
    // this mode when a newer status for `b` arrived -- and choosing it sent a realizing build for
    // `b`'s recipe while the sentence said `a`. Each row's own promise is kept beside the ask now
    // and established again when the answer lands.
    //
    // (X) MUTATION, MEASURED. `offer_menu` recording no advertised subject for
    //   `kMenuLoadBuilt`: the choice asks the tool for `two` and this case says so.
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
    // ...AND THE REFUSAL SAYS WHAT WENT, rather than leaving the maker to compare two names.
    CHECK_MESSAGE(b.text().find("`a` is not what is here now") != std::string::npos, b.text());
    CHECK(b.tool->asked.empty());
}

TEST_CASE("BLD-MOUSE: a numbered control naming an artifact is refused once that artifact is not what is standing") {
    // ⭐ THE REVIEW'S SECOND REPRODUCTION OF THE SAME FINDING (B2), AT THE OTHER SEAM. A
    // control's recorded meaning kept only its action id, so `[load built a]` and `[load built
    // b]` -- equal-width faces -- were the same span and kept the same picture number. A press
    // authenticated against the OLD picture therefore passed the fence and spent the newer
    // subject. Two things answer it: the advertised subject is part of the meaning, so the
    // picture moves when the promise does, and it is checked again when the press arrives.
    //
    // (X) MUTATIONS, MEASURED. `say_controls` recording `{}` for the subject again: the picture
    //   does not move, the old press passes the fence, and the tool is asked for `two`.
    //   `perform_on` without its comparison: the fence alone still catches this press, but the
    //   `(X)` below -- a host that numbers no picture -- spends it on `two`.
    BuilderRig b("bld-control-artifact");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}});
    b.open();
    std::int64_t picture_id = 0;
    const auto pane_id = b.r.bus.role_holder(pane::kBuilderPaneRole);
    const auto obs = b.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.sender == pane_id && ev.payload &&
            ev.schema_name == v3::PaneContent::zen_name) {
            picture_id = loom::from_value<v3::PaneContent>(*ev.payload).picture;
        }
    });
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const BpFaceAt aimed = bp_face_at(b.shown(), "[load built a]");
    REQUIRE(aimed.row >= 0);
    const auto old_picture = picture_id;
    REQUIRE(old_picture > 0);
    bp_settled(b, "two", "b", bld::outcome::kSucceeded);
    const auto changed_picture = picture_id;
    // THE PROMISE MOVED, SO THE PICTURE MOVED: equal-width faces are no longer equal spans.
    CHECK_MESSAGE(changed_picture != old_picture,
                  "the control's advertised subject changed and the picture did not");
    const std::string before_press = b.text();
    INFO("old picture ", old_picture, ", picture after status change ", changed_picture,
         "\nbefore old press\n", before_press);
    const auto sent = b.r.bus.office_send_to_role_as(
        b.r.workshop_id, kWorkshopProvider, pane::kBuilderPaneRole,
        loom::Message(loom::to_value(v3::PanePressed{pane::kBuilderPane, aimed.row,
                                                     aimed.column + 1, true, old_picture}),
                      b.r.workshop_id, b.r.workshop_id, 0));
    REQUIRE(sent.valid());
    b.r.bus.drain_until_idle();
    b.r.bus.remove_observer(obs);
    CHECK_MESSAGE((b.tool->asked.empty() || b.tool->asked.back() == "one"),
                  "Old load-a control instead requested ", bp_last_asked(b));
    CHECK(b.tool->asked.empty());
}

TEST_CASE("BLD-MOUSE: the face drawn where the older one was is the one a press spends") {
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
    bp_press_face(b, "[load built b]"); // the maker's press on what is drawn there now
    REQUIRE(b.tool->asked.size() == 1);
    CHECK(b.tool->asked.back() == "two"); // ...which is honest and acts
}

TEST_CASE("BLD-MOUSE: a held load menu cannot switch recipes sharing an artifact stem") {
    // ⭐ THE INDEPENDENT REVIEW'S FOLLOW-UP FINDING (C-B1), REPRODUCED AND REPAIRED. The two
    // cases above prove the fence catches a DIFFERENT artifact settling underneath an offer;
    // this one is the case WL-PROJ-14 explicitly supports and the artifact-name check alone
    // could not catch -- two recipes producing ONE stem. `one` and `two` both build `a`, so the
    // menu's advertised subject ("a") reads the same before and after the second build settles,
    // and only the operation behind it (`target_op_of`) tells the two builds apart.
    //
    // (X) MUTATION, MEASURED. `target_op_of` returning 0 unconditionally (or `perform_on`
    //   skipping the operation check): the name alone matches, and the stale choice asks the
    //   tool for `two` instead of refusing.
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

TEST_CASE("BLD-MOUSE: a numbered load control preserves the build behind a shared artifact stem") {
    // ⭐ THE SAME FOLLOW-UP FINDING (C-B2), AT THE CONTROL-MAP SEAM. `one` and `two` both build
    // `a`, so `[load built a]` is drawn at the same place, the same width and the same text
    // before and after the second build settles -- nothing about the FACE moved. Before the
    // operation joined the control's recorded meaning, the picture number did not move either,
    // so a press authenticated against the OLD picture passed the fence and spent the NEWER
    // build. The operation now rides in the meaning beside the subject (`BuilderMeaning::op`),
    // so an unmoved-looking control still bumps the picture when the build behind it changes.
    //
    // (X) MUTATIONS, MEASURED. `BuilderMeaning::operator==` ignoring `op`: two builds sharing a
    //   stem keep one picture number, the stale press passes `map_.current`, and (without the
    //   `perform_on` operation check too) the tool is asked for `two`. `controls[...].subject_op`
    //   recorded as 0: the same failure, reached through `say_controls` instead of the meaning.
    BuilderRig b("bld-control-shared-stem");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open();
    std::int64_t picture_id = 0;
    const auto pane_id = b.r.bus.role_holder(pane::kBuilderPaneRole);
    const auto obs = b.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.sender == pane_id && ev.payload &&
            ev.schema_name == v3::PaneContent::zen_name) {
            picture_id = loom::from_value<v3::PaneContent>(*ev.payload).picture;
        }
    });
    b.tool->next.op = 11;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const BpFaceAt aimed = bp_face_at(b.shown(), "[load built a]");
    REQUIRE(aimed.row >= 0);
    const auto old_picture = picture_id;
    REQUIRE(old_picture > 0);
    b.tool->next.op = 12;
    bp_settled(b, "two", "a", bld::outcome::kSucceeded);
    const auto changed_picture = picture_id;
    // THE FACE DID NOT MOVE -- same row, same column, same text -- and the picture still did,
    // because the operation behind the unmoved face changed.
    const BpFaceAt still_there = bp_face_at(b.shown(), "[load built a]");
    REQUIRE_MESSAGE(still_there.row == aimed.row, b.text());
    REQUIRE_MESSAGE(still_there.column == aimed.column, b.text());
    CHECK_MESSAGE(changed_picture != old_picture,
                  "two builds sharing an artifact stem kept one picture number");
    const auto sent = b.r.bus.office_send_to_role_as(
        b.r.workshop_id, kWorkshopProvider, pane::kBuilderPaneRole,
        loom::Message(loom::to_value(v3::PanePressed{pane::kBuilderPane, aimed.row,
                                                     aimed.column + 1, true, old_picture}),
                      b.r.workshop_id, b.r.workshop_id, 0));
    REQUIRE(sent.valid());
    b.r.bus.drain_until_idle();
    b.r.bus.remove_observer(obs);
    CHECK_MESSAGE((b.tool->asked.empty() || b.tool->asked.back() == "one"),
                  "An old numbered load control must refuse or preserve recipe one; observed ",
                  bp_last_asked(b));
    CHECK(b.tool->asked.empty());
}

TEST_CASE("BLD-MOUSE: rebuilding the same recipe replaces an offered load, and settling again does not") {
    // ⭐ THE PROMPT'S OWN EXTENSION OF C-B1/C-B2: the recipe and the artifact name both stay
    // `one` -> `a`, and only the OPERATION changes -- a maker who rebuilds while an old offer
    // stands must not have it silently spend the rebuild, but ordinary PROGRESS on one build (a
    // repeated status for the SAME operation, `still going` settling into `succeeded`) must
    // never be mistaken for a different one, or the offer would refuse itself.
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

TEST_CASE("BLD-MOUSE: the promote and revert controls refuse once the image they name is not the one standing") {
    // THE SAME BOUNDARY, ON THE OTHER TWO CONTROLS THAT NAME THEIR SUBJECT. What is STANDING
    // moves with every build that settles, so `[promote a]` is a promise with the same lifetime
    // as `[load built a]` and is judged the same way.
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
    // THE MAKER'S OWN PRESS ON WHAT IS DRAWN THERE NOW is honest and acts on `b`.
    bp_press_face(b, "[promote b]");
    REQUIRE(b.tool->promotes.size() == promoted + 1);
    CHECK(b.tool->promotes.back() == "b");
}

TEST_CASE("BLD-MOUSE: the promote and revert controls refuse a stale press across a shared artifact stem") {
    // THE SAME BOUNDARY THE PRIOR CASE PROVES BY NAME, NOW BY A STALE PICTURE -- the gap the
    // review's follow-up asked to have traced (R1's own words: "assess the related image
    // operations at the same boundary"). Two DIFFERENT recipes realizing ONE artifact stem draw
    // `[promote a]` and `[revert a]` in the same place, at the same width, before and after --
    // nothing about either FACE moves, so only the operation behind the meaning (`subject_op`)
    // tells the two realizations apart. `PromoteArtifact`/`RevertArtifact` carry only the
    // artifact name downstream, so this is the pane's own fence keeping a queued press from
    // spending whatever is CURRENTLY standing under that name -- not a claim that the message a
    // realization owner receives could itself tell the two builds apart.
    BuilderRig b("bld-promote-revert-shared-stem");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "a"}});
    b.open();
    b.author_height(30, 200, 60); // a room the whole strip fits in, `[promote a]` included
    std::int64_t picture_id = 0;
    const auto pane_id = b.r.bus.role_holder(pane::kBuilderPaneRole);
    const auto obs = b.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.sender == pane_id && ev.payload &&
            ev.schema_name == v3::PaneContent::zen_name) {
            picture_id = loom::from_value<v3::PaneContent>(*ev.payload).picture;
        }
    });
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
    const auto old_picture = picture_id;
    REQUIRE(old_picture > 0);

    // A DIFFERENT RECIPE REALIZES THE SAME ARTIFACT NAME: the faces read exactly as they did.
    b.tool->next.op = 42;
    b.tool->next.recipe = "two";
    b.tool->next.artifact = "a";
    b.tool->next.outcome = bld::outcome::kSucceeded;
    b.tool->next.realization = bld::realization::kRealized;
    b.tool_says();
    const auto changed_picture = picture_id;
    const BpFaceAt promote_still = bp_face_at(b.shown(), "[promote a]");
    const BpFaceAt revert_still = bp_face_at(b.shown(), "[revert a]");
    REQUIRE_MESSAGE(promote_still.row == promote_aimed.row, b.text());
    REQUIRE_MESSAGE(promote_still.column == promote_aimed.column, b.text());
    REQUIRE_MESSAGE(revert_still.row == revert_aimed.row, b.text());
    REQUIRE_MESSAGE(revert_still.column == revert_aimed.column, b.text());
    CHECK_MESSAGE(changed_picture != old_picture,
                  "two realizations sharing an artifact stem kept one picture number");

    const std::size_t promoted = b.tool->promotes.size();
    const auto press_at = [&](const BpFaceAt& at) {
        const auto sent = b.r.bus.office_send_to_role_as(
            b.r.workshop_id, kWorkshopProvider, pane::kBuilderPaneRole,
            loom::Message(loom::to_value(v3::PanePressed{pane::kBuilderPane, at.row,
                                                         at.column + 1, true, old_picture}),
                          b.r.workshop_id, b.r.workshop_id, 0));
        REQUIRE(sent.valid());
        b.r.bus.drain_until_idle();
    };
    press_at(promote_aimed);
    CHECK_MESSAGE(b.tool->promotes.size() == promoted,
                  "a stale promote press across a shared stem promoted anyway");

    const std::size_t reverted = b.tool->reverts.size();
    press_at(revert_aimed);
    CHECK_MESSAGE(b.tool->reverts.size() == reverted,
                  "a stale revert press across a shared stem reverted anyway");
    b.r.bus.remove_observer(obs);
}

TEST_CASE("BLD-MOUSE: `edit source` in the recipe list opens the row the list is standing on, and leaves the choice alone") {
    // ⭐ THE REVIEW'S SEVENTH FINDING (B3), REPRODUCED AND REPAIRED. The list menu offered
    // `edit `one`'s source` and the list's dispatcher had no branch for it, so the menu closed,
    // no Editor opened and no refusal said why. The operation reaches `edit_source` now, which
    // reads the LIST's cursor while the list is open -- and does not touch the committed choice
    // to make itself possible (WL-PROJ-14: looking is not choosing).
    //
    // (X) MUTATIONS, MEASURED. The `kActionEditSource` branch removed from `choose_in_list`: no
    //   Editor appears. `edit_source` reading `cursor_row()` again: the Editor opens `two.cpp`,
    //   which is the committed choice and not the row the menu named.
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
    REQUIRE(b.r.session().panels.has(b.editor_kind()));
    CHECK(b.editor_status().find("one.cpp") != std::string::npos);
    CHECK(b.editor_status().find("two.cpp") == std::string::npos);
    // ...AND THE COMMITTED CHOICE IS UNTOUCHED: opening a source chooses nothing.
    bp_press_face(b, "[close the list]");
    CHECK_MESSAGE(b.text().find("two -> b  (2/2)") != std::string::npos, b.text());
}

TEST_CASE("BLD-MOUSE: a capital letter typed into the Builder's role line is text, not this pane's menu") {
    // ⭐ THE REVIEW'S SECOND FINDING (B5), REPRODUCED AND REPAIRED. The pane declared its menu
    // on `Shift+M` in every mode. Workshop resolves the KEY TRANSITION against the declaration
    // before the character it produced arrives, so the shifted `M` of a role like `Main` opened
    // the menu and the letter was lost. The role line declares the menu with no default key now.
    //
    // (X) MUTATION, MEASURED. The role row declared on `kM`/`kShift` again: the menu opens and
    //   the line reads `x`, so both checks below fail.
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

TEST_CASE("BLD-MOUSE: the role line keeps typed text visible in a narrow room") {
    // THE SAME CONSTRUCTION AS FILES' NARROW AUTHORING FIELD, ON THE BUILDER'S OWN LINE
    // (`active_role_prompt`/`active_prompt`, the followup review's second finding, traced to
    // "the other authoring labels, and the Builder role line where the same construction
    // applies"). `role_prompt` grows with the stem being loaded, so a long artifact name in a
    // thirty-column room left nothing for the maker's typing to show in before this existed.
    // Widening afterward separates hidden text from lost text, exactly as Files' own case does.
    BuilderRig b("bld-role-narrow");
    b.tool->catalog = catalog_of({{"one", "zengine-really-long-example"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.r.key(input::scan::kO);
    REQUIRE_MESSAGE(b.text().find("type the role") != std::string::npos, b.text());

    const Written narrow = author_pane_size(b.r.session().setup.active, builder_ref(),
                                            PaneSize{pane_unit::kSubcells, subs(32)},
                                            PaneSize{pane_unit::kSubcells, subs(9)});
    REQUIRE_MESSAGE(narrow.accepted, narrow.refusal);
    b.r.extent(160, 47);
    b.r.text("narrowvalue");
    INFO("narrow role line\n", b.text());
    CHECK_MESSAGE(b.text().find("narrowvalue") != std::string::npos,
                  "the role line accepted text but its full prompt hid that text");

    // SEPARATE ACCEPTANCE FROM VISIBILITY: the same draft reveals its text once wider.
    const Written wide = author_pane_size(b.r.session().setup.active, builder_ref(),
                                          PaneSize{pane_unit::kSubcells, subs(80)},
                                          PaneSize{pane_unit::kSubcells, subs(9)});
    REQUIRE_MESSAGE(wide.accepted, wide.refusal);
    b.r.extent(160, 48);
    CHECK_MESSAGE(b.text().find("role for zengine-really-long-example> narrowvalue") !=
                      std::string::npos,
                  b.text());
}

TEST_CASE("BLD-MOUSE: a Builder menu choice that opens the role line takes the keyboard across the door it waits on") {
    // ⭐ THE REVIEW'S SIXTH FINDING, ON THIS PANE. A right press is deliberately focus-neutral,
    // so a maker whose keys are elsewhere could choose `add `one`'s artifact to the load plan`
    // and watch a role line open that no character could reach. This one is harder than Files':
    // the line opens only once the plan office has answered, one delivery later, so the
    // choice's own number rides across that round trip and the grab is spent there.
    //
    // (X) MUTATIONS, MEASURED. `names_.choice` never set: custody stays where it was and the
    //   typed role reaches nothing. `take_keys` called in `chose` instead: the grab is spent
    //   before the line exists, and the role line opens with the keys on a pane that is not
    //   showing one.
    BuilderRig b("bld-menu-takes-keys");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.unfocus();
    REQUIRE(b.r.session().panels.keyboard != b.kind);
    const std::int64_t recipe = bp_row(b.shown(), "recipe   one -> a");
    REQUIRE_MESSAGE(recipe >= 0, b.text());
    bp_button(b.r, b.kind, 3, true, recipe, 0);
    REQUIRE(menu_shown(b.r.session()));
    CHECK(b.r.session().panels.keyboard != b.kind); // pointing is not typing
    bp_choose_row(b, "add `one`'s artifact to the load plan...");
    REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
    CHECK(b.r.session().panels.keyboard == b.kind);
    b.r.text("example.tally");
    CHECK_MESSAGE(b.text().find("role for a> example.tally") != std::string::npos, b.text());
}

TEST_CASE("BLD-MOUSE: a Builder menu choice that opens no line leaves the keyboard where the maker put it") {
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
    CHECK(b.r.session().panels.keyboard != b.kind);                    // ...the keys did not move
}

TEST_CASE("BLD-MOUSE: every control each Builder mode draws has a row in that mode's own menu") {
    // ⭐ THE STRIP'S PROMISE, KEPT. A narrow strip drops what will not fit and writes
    // `+N in menu`; that sentence is true only if the menu carries the mode's whole list. A
    // control the mode draws UNAVAILABLE gets its row too, so a maker who cannot reach an
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
                                "manage this pane..."}) {
            CHECK_MESSAGE(any_row(offered, row), "the Builder menu has no row `", row, "`");
        }
    }
    SUBCASE("the recipe list") {
        bp_press_face(b, "[choose a recipe...]");
        const std::vector<std::string> offered = bp_open_menu(b);
        INFO("offered\n", bp_picture(offered));
        for (const char* row : {"choose `one`", "edit `one`'s source",
                                "leave the choice as it was", "manage this pane..."}) {
            CHECK_MESSAGE(any_row(offered, row), "the list menu has no row `", row, "`");
        }
    }
    SUBCASE("the role line") {
        b.r.key(input::scan::kO);
        REQUIRE_MESSAGE(b.text().find("type the role it holds") != std::string::npos, b.text());
        const std::vector<std::string> offered = bp_open_menu(b);
        INFO("offered\n", bp_picture(offered));
        for (const char* row : {"load `a` with the role typed", "write nothing and load nothing",
                                "manage this pane..."}) {
            CHECK_MESSAGE(any_row(offered, row), "the role menu has no row `", row, "`");
        }
    }
}

TEST_CASE("BLD-MOUSE: the list's own double-click still takes the row it was aimed at") {
    // ⚠ THE REPAIR ABOVE COULD HAVE COST THIS, and this case is why the list's two faces name no
    // recipe. A face reading `choose `one`` would move this strip's spans every time the maker
    // looked at another row, and the picture fence would then refuse the second press of an
    // ordinary double-click -- which is P-WORK-25's own defect, reintroduced one pane over.
    BuilderRig b("bld-list-double-click");
    b.tool->catalog = catalog_of({{"one", "a"}, {"two", "b"}, {"three", "c"}});
    b.open();
    std::int64_t picture_id = 0;
    const auto pane_id = b.r.bus.role_holder(pane::kBuilderPaneRole);
    const auto obs = b.r.bus.add_observer([&](const loom::BusEvent& ev) {
        if (ev.kind == loom::EventKind::Delivered && ev.sender == pane_id && ev.payload &&
            ev.schema_name == v3::PaneContent::zen_name) {
            picture_id = loom::from_value<v3::PaneContent>(*ev.payload).picture;
        }
    });
    bp_press_face(b, "[choose a recipe...]");
    // THE LIST'S OWN OPENING SENTENCE IS SPENT FIRST, because spending a notice moves every row
    // under it -- which is a different fact from the one this case is about.
    press_pane(b.r, b.kind, bp_row(b.shown(), "  two -> b"), 0);
    const std::int64_t row = bp_row(b.shown(), "  three -> c");
    REQUIRE_MESSAGE(row >= 0, b.text());
    const std::int64_t before = picture_id;
    REQUIRE(before > 0);
    press_pane(b.r, b.kind, row, 0); // the first names the row
    REQUIRE_MESSAGE(bp_row(b.shown(), "> three -> c") == row, b.text());
    // ⭐ THE PICTURE DID NOT MOVE UNDER THE HAND. This is the whole claim: the list's faces name
    // no recipe, so moving the cursor changes no span, so the second press of the double-click
    // is still about the picture it was aimed at.
    CHECK_MESSAGE(picture_id == before, "the list's picture moved when its cursor did");
    press_pane(b.r, b.kind, row, 0); // ...and the second takes it, from the same picture
    b.r.bus.remove_observer(obs);
    CHECK_MESSAGE(b.text().find("build recipe: three -> c") != std::string::npos, b.text());
    CHECK_MESSAGE(b.text().find("three -> c  (3/3)") != std::string::npos, b.text());
}

TEST_CASE("BLD-MOUSE: a reader waiting on its first page still offers its whole list in a narrow room") {
    // ⭐ THE REVIEW'S FIFTH FINDING (B4), IN THE STATE IT REPRODUCED IT IN: a reader whose page
    // the Builder has not answered draws a header and nothing else, and a narrow strip then
    // leaves `[menu]` as the only route there is. The menu carried `close` and `manage` alone.
    // The reader's own operations over REAL retained output are in the output suite; what this
    // case adds is the state a fixture tool can hold open and a real one cannot.
    //
    // (X) MUTATION, MEASURED. The reader's branch of `offer_menu` reduced to `kMenuClose` again:
    //   both checks below fail, and the maker in this room has no operation but close.
    BuilderRig b("bld-reader-waiting");
    b.tool->catalog = catalog_of({{"one", "a"}});
    b.open(160, 48, /*with_editor=*/false, /*with_manager=*/true, /*with_project_door=*/true,
           /*with_presenter=*/true);
    b.tool->next.op = 1;
    bp_settled(b, "one", "a", bld::outcome::kSucceeded);
    const Written sized = author_pane_size(b.r.session().setup.active, builder_ref(),
                                           PaneSize{pane_unit::kSubcells, subs(30)},
                                           PaneSize{pane_unit::kSubcells, subs(9)});
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
