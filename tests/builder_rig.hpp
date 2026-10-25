// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss
#ifndef ZENGINE_TESTS_BUILDER_RIG_HPP
#define ZENGINE_TESTS_BUILDER_RIG_HPP
// A LIVE WORKSHOP WITH THE REAL BUILDER PANE LOADED INTO IT: a stand-in for the Builder tool in its
// office, the project door, and the real `zengine-builder-pane` image
// over the real pane protocol, with the shipped presenter, desktop or Editor beside it when a case
// asks. The Builder suite's cases and every case about a guest teaching on the Builder start here.
#include "workshop_support.hpp"

#include "builder/builder-pane/vocabulary.hpp"
#include "workshop/authoring.hpp"
#include "workshop/builder_seam_vocabulary.hpp"
#include "workshop/desktop-pane/vocabulary.hpp"
#include "workshop/pane_doors.hpp"

#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace builder_rig {

namespace pane = zengine::builder_pane;
namespace bld = zengine::builder;

/// The office and pane a saved setup names, spelled through the package's own header --
/// the durable names, not literals, so a case cannot agree with a typo.
inline PaneRef builder_ref() {
    return PaneRef{pane::kBuilderPaneRole, pane::kBuilderPane};
}

/// A STAND-IN FOR THE BUILDER TOOL, holding its office and answering as the real one does. These
/// cases measure what the PANE asks for and makes of an answer, so the answering side is one a
/// case can set and count; the real tool is `test_builder.cpp`'s, against a real compiler.
struct ToolState {
    std::int64_t answered = 0; ///< the weave's own shape; the counters a case reads are below
    ZEN_SHAPE(ToolState, 1, ZEN_FIELD(answered));
};

class Tool
    : public loom::WeaveBase<Tool, ToolState,
                             loom::Accept<bld::StatusRequested, bld::BuildRequested,
                                          bld::PromoteArtifact, bld::RevertArtifact,
                                          bld::BuildOutputRequested, SeatDo>,
                             loom::Emit<bld::BuildStatus, bld::RecipeCatalog>> {
public:
    bld::RecipeCatalog catalog{};
    bld::BuildStatus next{};
    /// PLAIN MEMBERS, NOT STATE -- a case reads them directly.
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
        ++state_.answered;
        asked.push_back(ask.recipe);
        realize_asked.push_back(ask.realize);
        if (busy) {
            // ONE AT A TIME: refused in the tool's own voice, its picture the running build's.
            (void)mail.publish(next);
            return;
        }
        ++builds;
        next.recipe = ask.recipe;
        next.realize = ask.realize;
        next.builds = builds;
        next.op = 0;
        next.outcome = bld::outcome::kAsked;
        next.realization = ask.realize ? bld::realization::kAsked : bld::realization::kNotAsked;
        next.realized_detail.clear();
        (void)mail.publish(next);
    }
    /// A build is running that the tool will not leave for another ask.
    bool busy = false;
    void on(const bld::PromoteArtifact& said, loom::Mail&) { promotes.push_back(said.artifact); }
    void on(const bld::RevertArtifact& said, loom::Mail&) { reverts.push_back(said.artifact); }
    /// A READER'S PAGE, HEARD AND NOT ANSWERED: the page waits on a tool that holds no output.
    void on(const bld::BuildOutputRequested&, loom::Mail&) { ++pages_asked; }
    std::int64_t pages_asked = 0;

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

/// A live Workshop with the real Builder pane loaded into it. The tool and the doors are mounted
/// before the plan runs, because the pane asks both on the beat it is first granted room -- a door
/// mounted afterwards would be absent exactly when that ask is made.
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

    /// `with_editor` loads the real Editor image, so the second door `e` walks has an answerer;
    /// `with_manager` mounts the opening manager (WL-OPEN-01) and `with_project_door` the
    /// read-only project office, each left out a door that reaches nobody -- the
    /// refusal-at-dispatch cases' subject. `with_presenter` puts the shipped presenter in the plan,
    /// as a plan row, and `with_desktop` the shipped desktop, whose Manual a pane's `?` shows.
    void open(std::int64_t width = 160, std::int64_t height = 48, bool with_editor = false,
              bool with_manager = true, bool with_project_door = true,
              bool with_presenter = false, bool with_desktop = false) {
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
        if (with_desktop) {
            load::ArtifactIntent desktop;
            desktop.stem = zengine::desktop_pane::kDesktopStem;
            desktop.weave = load::WeaveIntent{kDesktopRole};
            plan.artifacts.push_back(desktop);
        }
        const load::Executed done = r.run_plan(plan);
        REQUIRE_MESSAGE(done.ok, done.refusal);
        r.ready();
        r.extent(width, height);
        REQUIRE_MESSAGE(row() != nullptr, "the loaded image offered no `builder` pane");
        r.pick(builder_ref());
        kind = row()->kind;
        REQUIRE(author_pane_size(r.session().setup.active, builder_ref(),
            {pane_unit::kPixels, cells_px(placement_bounds(placement::kOverlayStack, 0,
                screen_of(cells_px(width), cells_px(height))).w)}, PaneSize{}).accepted);
        focus();
    }

    /// PRESS INTO THE PANE: its rows are active only while it holds the keyboard, so every
    /// gesture below is a weaver who has pointed at the Builder first. Row 0 of the room is the
    /// pane's own first row.
    void focus() {
        const CellRect body = external_body_rect(r.session(), kind);
        r.press_cell(body.x, body.y);
        REQUIRE(r.session().panes.keyboard == kind);
    }

    /// A TALLER PANE, authored the way a weaver's setup file authors one: some cases need the
    /// whole control strip drawn, and the strip grows with the room.
    void author_height(std::int64_t cells, std::int64_t width, std::int64_t height) {
        const Written wrote = author_pane_size(r.session().setup.active, builder_ref(),
            {pane_unit::kPixels, cells_px(placement_bounds(placement::kOverlayStack, 0,
                screen_of(cells_px(width), cells_px(height))).w)}, {pane_unit::kPixels, cells_px(cells)});
        REQUIRE_MESSAGE(wrote.accepted, wrote.refusal);
        r.extent(width, height);
    }

    /// Hand the keys back the way a weaver does -- a press on the bare workspace.
    void unfocus() {
        r.press_cell(0, gfx::cell_of_pixel(screen_of(r.session()).h) - 1);
        REQUIRE(r.session().panes.keyboard != kind);
    }

    const RuntimePane* row() {
        return r.session().panes.runtime.find(pane::kBuilderPaneRole, pane::kBuilderPane);
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
        const RuntimePane* editor = r.session().panes.runtime.find("zengine.editor", "editor");
        REQUIRE(editor != nullptr);
        return editor->kind;
    }
    std::string editor_status() {
        const std::vector<std::string> rows = pane_rows(r, editor_kind());
        REQUIRE_FALSE(rows.empty());
        return rows[0];
    }

    std::vector<std::string> shown() { return pane_rows(r, kind); }

    /// Every row the pane published, joined -- what a weaver reads at the pane's rectangle.
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

    /// A NEW ROOM AND NOTHING ELSE: the surface changes size, the window gives the pane's area
    /// its room again, and the pane says its rows -- the ordinary repaint that exposes a notice
    /// cleared in private. Required to be a real room, so a deduplicated extent cannot pass for one.
    void regrant() {
        const ExternalPane* seat = r.session().panes.external_pane(kind);
        REQUIRE(seat != nullptr);
        const std::int64_t rows = held_canvas_rows(*seat).rows;
        const std::int64_t columns = held_canvas_rows(*seat).columns;
        wide_ = !wide_;
        author_test_pane_room(r, kind, rows + 1, columns);
        r.extent(wide_ ? 160 : 150, wide_ ? 48 : 44);
        const ExternalPane* after = r.session().panes.external_pane(kind);
        REQUIRE(after != nullptr);
        REQUIRE_MESSAGE((held_canvas_rows(*after).rows != rows ||
                         held_canvas_rows(*after).columns != columns),
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
        for (const v4::PaneActionRow& a : seat->actions) {
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

    /// The project office `mount_doors` seated, when it seated one.
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

        auto project = std::make_unique<ProjectDoor>(
            r.host.project_dir, marks_, r.host.frontier, r.host.plan_names, r.host.recipe_source,
            ProjectDoor::Use{}, ProjectDoor::Author{}, r.host.append_plan_row);
        ProjectDoor* praw = project.get();
        loom::Grant say_project;
        say_project.allow_to_any(ProjectRoot::zen_name, ProjectRoot::zen_version);
        say_project.allow_to_any(ProjectFrontierSaid::zen_name, ProjectFrontierSaid::zen_version);
        say_project.allow_to_any(PlanNames::zen_name, PlanNames::zen_version);
        say_project.allow_to_any(RecipeSourceSaid::zen_name, RecipeSourceSaid::zen_version);
        say_project.allow_to_any(PlanRowWritten::zen_name, PlanRowWritten::zen_version);
        const loom::WeaveId pid = r.bus.register_weave(std::move(project), std::move(say_project),
                                                       std::string(kProjectRole));
        praw->zen_set_self(pid);
        project_id = pid;
    }

private:
    std::string marks_;
};

inline bld::RecipeCatalog catalog_of(std::vector<std::pair<std::string, std::string>> rows,
                                     std::string source = "/project/recipes.json") {
    bld::RecipeCatalog out;
    for (const auto& row : rows) {
        out.recipes.push_back(bld::RecipeSummary{row.first, row.second});
    }
    out.source = std::move(source);
    return out;
}

} // namespace builder_rig

#endif // ZENGINE_TESTS_BUILDER_RIG_HPP
