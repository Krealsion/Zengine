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
    void open(std::int64_t width = 160, std::int64_t height = 48, bool with_editor = false,
              bool with_manager = true, bool with_project_door = true) {
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
        r.host.recipe_source = [this](const std::string&) { return next_source; };
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

/// The nine ids the pane declares while it is browsing, and the two its role line does --
/// spelled through the package's own header so a case cannot agree with a typo.
const std::vector<std::string> kBrowsingIds = {
    pane::kActionBuild,      pane::kActionBuildRealize, pane::kActionPromote,
    pane::kActionRevert,     pane::kActionLoadIt,       pane::kActionRecipeNext,
    pane::kActionRecipeBack, pane::kActionFrontier,     pane::kActionEditSource};

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
    CHECK(b.text().find("loads zengine-snake now") != std::string::npos);

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
    // ...AND WHILE THE LINE HAS THE KEYBOARD THE PANE DECLARES TWO ROWS AND NO MORE, so
    // every other key reaches it as an ordinary keystroke for the line to consume
    // (WL-FILES-16, one pane over).
    const RuntimePane* seat = b.row();
    REQUIRE(seat != nullptr);
    REQUIRE(seat->actions.size() == 2);
    CHECK(seat->actions[0].id == std::string(pane::kActionCommit));
    CHECK(seat->actions[1].id == std::string(pane::kActionCancel));

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
    REQUIRE(b.row()->actions.size() == kBrowsingIds.size());
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
    REQUIRE(b.row()->actions.size() == kBrowsingIds.size());
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
    CHECK(b.text().find("names no single source") != std::string::npos);

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

TEST_CASE("BLD-WEAVE: a lookup nothing could queue is refused at once in words, an open queued to an office nobody holds is refused at dispatch by that attempt, and a fresh e takes once each office is present") {
    // THE FIRST DOOR CANNOT EVEN BE ASKED: no project office is held, and with it absent
    // nobody on this bus declares the lookup's shape, so the seam admits nothing and the
    // pane's ticket is not valid -- the IMMEDIATE enqueue refusal, handled at once, in words
    // naming the lookup as the stage that failed. (Not silence, and never a fabricated stage.
    // A lookup that WAS queued and then refused at dispatch is the next case's.)
    BuilderRig a("bld-lookup-refused");
    a.tool->catalog = catalog_of({{"snake", "zengine-snake"}});
    a.open(160, 48, /*with_editor=*/true, /*with_manager=*/true, /*with_project_door=*/false);
    a.letter(input::scan::kE, "e");
    CHECK_MESSAGE(a.text().find("`snake`") != std::string::npos, a.text());
    CHECK_MESSAGE(a.text().find("could not be looked up") != std::string::npos, a.text());
    CHECK_MESSAGE(a.text().find("nothing was queued") != std::string::npos, a.text());
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
    const std::vector<std::string> line_ids{pane::kActionCommit, pane::kActionCancel};
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
    CHECK(b.declared().size() == kBrowsingIds.size());

    // ESCAPE AND RETURN IN ONE POLL: the cancel, then a commit Workshop resolved against the
    // line's rows, arriving after the line closed. The cancel's answer stands.
    b.letter(input::scan::kO, "o");
    REQUIRE(b.declared() == line_ids);
    b.enqueue_key(input::scan::kEscape);
    b.enqueue_key(input::scan::kReturn);
    b.settle();
    CHECK(b.declared().size() == kBrowsingIds.size());
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
