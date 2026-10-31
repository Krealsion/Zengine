// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DOORS_HPP
#define ZENGINE_WORKSHOP_PANE_DOORS_HPP

// The host's side of the pane seam: the project door a pane weave asks, which reads four facts
// and spends three acts. It derives and calls and remembers nothing. Only an office may ask, which
// names nobody and is not containment: the loader binds `allow_any()` to every library it opens.

#include "builder_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "weave.hpp"

#include "activation/activation.hpp"
#include "manual/vocabulary.hpp"
#include "manuals/zengine.project.hpp" // the office's manual, compiled in from its page

#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace zengine::workshop {

/// What the project door has done: counters only, since an answer kept between asks would be the
/// mirror this door exists not to build. The acts are told apart from the owners' refusals of
/// them: "a weaver asked" and "the project would not take it" are two facts.
struct ProjectDoorState {
    std::int64_t answers = 0;   ///< where this run began, and where its marks live
    std::int64_t frontiers = 0; ///< what realization is waiting on
    std::int64_t lookups = 0;   ///< whether the plan in force names an artifact
    std::int64_t sources = 0;   ///< which file a recipe names
    std::int64_t used = 0;      ///< recipe files asked to be the one in force
    std::int64_t authored = 0;  ///< recipe rows asked to be written
    std::int64_t planned = 0;   ///< plan rows asked to be added
    std::int64_t refusals = 0;  ///< acts the owner refused, in its own words
    std::int64_t refused = 0;   ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(ProjectDoorState, 3, ZEN_FIELD(answers), ZEN_FIELD(frontiers),
              ZEN_FIELD(lookups), ZEN_FIELD(sources), ZEN_FIELD(used), ZEN_FIELD(authored),
              ZEN_FIELD(planned), ZEN_FIELD(refusals), ZEN_FIELD(refused));
};

/// The project office: where this run began, what realization is waiting on, whether the plan
/// names an artifact and which file a recipe names, read; and which recipes the project means and
/// which artifacts it loads, changed. The reads run nobody's code. Each act spends the one closure
/// the host wired for it, so a recipe and a plan row stay two acts with two owners, and the door
/// re-words no refusal.
class ProjectDoor
    : public zengine::With<manual::ManualFacet<manual::pages::zengine_project::kManual>>::Weave<
          ProjectDoor, ProjectDoorState,
          loom::Accept<ProjectRootRequested, ProjectFrontierRequested, PlanNamesRequested,
                       RecipeSourceRequested, RecipeUseRequested, RecipeAuthorRequested,
                       PlanRowRequested>,
          loom::Emit<ProjectRoot, ProjectFrontierSaid, PlanNames, RecipeSourceSaid, RecipeOutcome,
                     PlanRowWritten>> {
public:
    using Frontier = std::function<ProjectFrontier()>;
    using Names = std::function<bool(const std::string&)>;
    using Source = std::function<HostContext::RecipeSource(const std::string&)>;
    using Use = std::function<HostContext::RecipeSwap(const std::string&)>;
    using Author = std::function<HostContext::RecipeSwap(const HostContext::RecipeDraft&)>;
    using Append = std::function<HostContext::PlanAppend(
        const std::string&, const std::string&, const std::string&)>;

    ProjectDoor(const std::string& project_dir, const std::string& marks_path,
                Frontier frontier = {}, Names names = {}, Source source = {}, Use use = {},
                Author author = {}, Append append = {})
        : project_dir_(&project_dir), marks_path_(&marks_path), frontier_(std::move(frontier)),
          names_(std::move(names)), source_(std::move(source)), use_(std::move(use)),
          author_(std::move(author)), append_(std::move(append)) {}

    void on(const ProjectRootRequested&, loom::Mail& mail) {
        if (!answerable(mail)) {
            return; // personal speech, or a root: nobody to be answerable to
        }
        ++state_.answers;
        // ANSWERED, NOT SENT: `mail.answer` is Loom's own door, so the recipient and the
        // correlation are the bus's and the asker reads `answers_ask()` off provenance no
        // payload can write. One ask, one answer.
        (void)mail.answer(ProjectRoot{*project_dir_, *marks_path_});
    }

    /// What realization is stopped on, derived at the ask (`HostContext::frontier`). A host that
    /// wired none answers the designed absence: `waiting` false, with an empty artifact.
    void on(const ProjectFrontierRequested&, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.frontiers;
        const ProjectFrontier now = frontier_ ? frontier_() : ProjectFrontier{};
        (void)mail.answer(ProjectFrontierSaid{now.waiting, now.artifact,
                                              static_cast<std::int64_t>(now.blocked)});
    }

    /// DOES THE PLAN IN FORCE ALREADY NAME THIS ARTIFACT? `HostContext::plan_names`, spent
    /// at the ask. The stem rides back on the answer so the asker reads it against the row
    /// it asked about rather than against wherever its cursor has since moved.
    void on(const PlanNamesRequested& asked, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.lookups;
        (void)mail.answer(PlanNames{asked.stem, names_ ? names_(asked.stem) : false});
    }

    /// Which file this recipe names (`HostContext::recipe_source`, spent at the ask): the one
    /// absolute path the recipe was completed to, or the owner's refusal in its own words.
    /// Opening the file is the Editor's, and is asked of the Editor.
    void on(const RecipeSourceRequested& asked, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.sources;
        if (!source_) {
            (void)mail.answer(RecipeSourceSaid{
                asked.recipe, false,
                "this host resolves no recipe sources -- nothing was opened", std::string()});
            return;
        }
        const HostContext::RecipeSource named = source_(asked.recipe);
        if (!named.known) {
            (void)mail.answer(RecipeSourceSaid{asked.recipe, false,
                                               "this project's recipes do not hold `" +
                                                   asked.recipe + "` -- nothing was opened",
                                               std::string()});
            return;
        }
        if (named.source.empty()) {
            (void)mail.answer(RecipeSourceSaid{asked.recipe, false,
                                               "`" + asked.recipe + "` is a " + named.kind +
                                                   " recipe -- it names no source file or "
                                                   "editing entry to open",
                                               std::string()});
            return;
        }
        (void)mail.answer(RecipeSourceSaid{asked.recipe, true, std::string(), named.source});
    }

    /// USE THIS FILE AS THE CATALOG. Every judgement about the bytes is the recipe owner's;
    /// this door reads none of them and re-words no refusal.
    void on(const RecipeUseRequested& asked, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.used;
        if (!use_) {
            (void)mail.answer(RecipeOutcome{false, "this host cannot change recipe catalogs",
                                            std::string(), 0});
            return;
        }
        answer(mail, use_(asked.path));
    }

    /// AUTHOR ONE ROW. The draft's fields become `HostContext::RecipeDraft` and go to the one
    /// writer (`authoring::author_recipe`), which owns the recipe law, the file the row lands
    /// in, the atomic save and the install.
    void on(const RecipeAuthorRequested& asked, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.authored;
        if (!author_) {
            (void)mail.answer(
                RecipeOutcome{false, "this host cannot author recipes", std::string(), 0});
            return;
        }
        HostContext::RecipeDraft draft;
        draft.id = asked.id;
        draft.artifact = asked.artifact;
        draft.source = asked.source;
        draft.packages = asked.packages;
        draft.links = asked.links;
        draft.build_dir = asked.build_dir;
        draft.target = asked.target;
        draft.artifact_dir = asked.artifact_dir;
        draft.config = asked.config;
        draft.tree = asked.tree;
        draft.makes = asked.makes;
        answer(mail, author_(draft));
    }

    /// ADD ONE ROW TO THE LOAD PLAN: `authoring::append_plan_row`, spent at the ask. A catalog
    /// says how an artifact is produced and a plan which ones run; the two closures keep them
    /// two acts.
    void on(const PlanRowRequested& asked, loom::Mail& mail) {
        if (!answerable(mail)) {
            return;
        }
        ++state_.planned;
        if (!append_) {
            (void)mail.answer(PlanRowWritten{false, "this host cannot author plan rows",
                                             std::string(), false, std::string(),
                                             std::string()});
            return;
        }
        const HostContext::PlanAppend done = append_(asked.stem, asked.role, asked.recipe);
        if (!done.accepted) {
            ++state_.refusals;
        }
        (void)mail.answer(PlanRowWritten{done.accepted, done.refusal, done.detail, done.frontier,
                                         done.product, done.path});
    }

private:
    bool answerable(loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            ++state_.refused;
            return false;
        }
        return true;
    }

    void answer(loom::Mail& mail, const HostContext::RecipeSwap& done) {
        if (!done.accepted) {
            ++state_.refusals;
        }
        (void)mail.answer(RecipeOutcome{done.accepted, done.refusal, done.path,
                                        static_cast<std::int64_t>(done.recipes)});
    }

    const std::string* project_dir_;
    const std::string* marks_path_;
    Frontier frontier_;
    Names names_;
    Source source_;
    Use use_;
    Author author_;
    Append append_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DOORS_HPP
