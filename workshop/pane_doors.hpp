// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DOORS_HPP
#define ZENGINE_WORKSHOP_PANE_DOORS_HPP

// The host's side of the pane seam: the doors a pane weave asks, one that reads and two that act.
// They derive and call and remember nothing. Only an office may ask, which names nobody and is
// not containment: the loader binds `allow_any()` to every library it opens.

#include "builder_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "weave.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace zengine::workshop {

/// What the project door has done: counters only, since an answer kept between asks would be the
/// mirror these doors exist not to build.
struct ProjectDoorState {
    std::int64_t answers = 0;   ///< where this run began, and where its marks live
    std::int64_t frontiers = 0; ///< what realization is waiting on
    std::int64_t lookups = 0;   ///< whether the plan in force names an artifact
    std::int64_t sources = 0;   ///< which file a recipe names
    std::int64_t refused = 0;   ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(ProjectDoorState, 2, ZEN_FIELD(answers), ZEN_FIELD(frontiers),
              ZEN_FIELD(lookups), ZEN_FIELD(sources), ZEN_FIELD(refused));
};

/// The read-only project office: where this run began, what realization is waiting on, whether
/// the plan names an artifact, and which file a recipe names. None of the four runs anybody's
/// code; an answer that changed the project would belong at an acting office.
class ProjectDoor
    : public loom::WeaveBase<
          ProjectDoor, ProjectDoorState,
          loom::Accept<ProjectRootRequested, ProjectFrontierRequested, PlanNamesRequested,
                       RecipeSourceRequested>,
          loom::Emit<ProjectRoot, ProjectFrontierSaid, PlanNames, RecipeSourceSaid>> {
public:
    using Frontier = std::function<ProjectFrontier()>;
    using Names = std::function<bool(const std::string&)>;
    using Source = std::function<HostContext::RecipeSource(const std::string&)>;

    ProjectDoor(const std::string& project_dir, const std::string& marks_path,
                Frontier frontier = {}, Names names = {}, Source source = {})
        : project_dir_(&project_dir), marks_path_(&marks_path), frontier_(std::move(frontier)),
          names_(std::move(names)), source_(std::move(source)) {}

    void on(const ProjectRootRequested&, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            ++state_.refused;
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
        if (mail.authored_role().empty()) {
            ++state_.refused;
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
        if (mail.authored_role().empty()) {
            ++state_.refused;
            return;
        }
        ++state_.lookups;
        (void)mail.answer(PlanNames{asked.stem, names_ ? names_(asked.stem) : false});
    }

    /// Which file this recipe names (`HostContext::recipe_source`, spent at the ask): the one
    /// absolute path the recipe was completed to, or the owner's refusal in its own words.
    /// Opening the file is the Editor's, and is asked of the Editor.
    void on(const RecipeSourceRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            ++state_.refused;
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

private:
    const std::string* project_dir_;
    const std::string* marks_path_;
    Frontier frontier_;
    Names names_;
    Source source_;
};

/// WHAT THE RECIPES DOOR HAS DONE. Counters, and the two acts told apart, because "a maker
/// chose a catalog" and "a maker authored a row" are two gestures.
struct RecipesDoorState {
    std::int64_t used = 0;
    std::int64_t authored = 0;
    std::int64_t refusals = 0; ///< acts the owner refused, in its own words
    std::int64_t refused = 0;  ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(RecipesDoorState, 1, ZEN_FIELD(used), ZEN_FIELD(authored), ZEN_FIELD(refusals),
              ZEN_FIELD(refused));
};

/// WHICH RECIPES THIS PROJECT MEANS -- the one office that may change them.
class RecipesDoor
    : public loom::WeaveBase<RecipesDoor, RecipesDoorState,
                             loom::Accept<RecipeUseRequested, RecipeAuthorRequested>,
                             loom::Emit<RecipeOutcome>> {
public:
    using Use = std::function<HostContext::RecipeSwap(const std::string&)>;
    using Author = std::function<HostContext::RecipeSwap(const HostContext::RecipeDraft&)>;

    RecipesDoor(Use use, Author author) : use_(std::move(use)), author_(std::move(author)) {}

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
        answer(mail, author_(draft));
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

    Use use_;
    Author author_;
};

/// WHAT THE PLAN DOOR HAS DONE. Counters, and the owner's refusals told from the asks: "a
/// maker asked to load an artifact" and "the running project would not take it" are two
/// facts, and a door that counted them as one would answer neither.
struct PlanDoorState {
    std::int64_t authored = 0;
    std::int64_t refusals = 0; ///< rows the owner refused, in its own words
    std::int64_t refused = 0;  ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(PlanDoorState, 1, ZEN_FIELD(authored), ZEN_FIELD(refusals), ZEN_FIELD(refused));
};

/// Which artifacts this project loads: the one office that may change them. Not `RecipesDoor`: a
/// catalog says how an artifact is produced and a plan which ones run, and one office writing
/// both would make one act two. It spends `authoring::append_plan_row` at the ask, and does
/// nothing else.
class PlanDoor : public loom::WeaveBase<PlanDoor, PlanDoorState,
                                        loom::Accept<PlanRowRequested>,
                                        loom::Emit<PlanRowWritten>> {
public:
    using Append = std::function<HostContext::PlanAppend(
        const std::string&, const std::string&, const std::string&)>;

    explicit PlanDoor(Append append) : append_(std::move(append)) {}

    void on(const PlanRowRequested& asked, loom::Mail& mail) {
        if (mail.authored_role().empty()) {
            ++state_.refused;
            return; // personal speech, or a root: nobody to be answerable to
        }
        ++state_.authored;
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
    Append append_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PANE_DOORS_HPP
