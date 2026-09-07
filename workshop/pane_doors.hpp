// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PANE_DOORS_HPP
#define ZENGINE_WORKSHOP_PANE_DOORS_HPP

// THE HOST'S SIDE OF THE PANE SEAM -- the doors a pane weave asks, one that reads and two
// that act.
//
//     ProjectDoor   zengine.project   ProjectRootRequested     -> ProjectRoot
//                                     ProjectFrontierRequested -> ProjectFrontierSaid
//                                     PlanNamesRequested       -> PlanNames
//     RecipesDoor   zengine.recipes   RecipeUseRequested       -> RecipeOutcome
//                                     RecipeAuthorRequested    -> RecipeOutcome
//     PlanDoor      zengine.plan      PlanRowRequested         -> PlanRowWritten
//
// The remaining doors -- one source opened in the Editor, by path or by recipe name -- are
// not here: Workshop's own weave holds the Editor, so `OpenSourceRequested` and
// `RecipeSourceRequested` are answered where `open_source` lives (`weave_pane_editor.cpp`),
// at the office Workshop already holds.
//
// ⚠ THE FILE WAS NAMED FOR THE FIRST PANE THAT ASKED, AND THE OFFICES NEVER WERE. The
// project browser's migration cut this seam and these doors carried its name through two
// migrations; the Builder was the second tenant, and added a shape to the read-only office
// and one office of its own rather than a second address for a question already answered
// here. `zengine.project` is THE read-only project office and was never Files' -- which is
// exactly why a second reader could arrive without editing what the first one asks, and why
// the file is called what it is now. Nothing about the doors changed with the rename.
//
// ---- THEY DERIVE AND CALL; THEY DO NOT REMEMBER --------------------------------
//
// `ProjectDoor` holds two `const std::string&` into the host's `main` and answers with
// copies of them, `ArrangementDoor`'s own shape: no store, no cache, nothing to update when
// anything changes. `RecipesDoor` holds the two closures the host already wired over its
// recipe owner (`use_recipes`, `author_recipe` -- `workshop/authoring.hpp`'s one writer) and
// spends them at the moment of the ask, `SampleDoor`'s shape: the act is the caller's
// gesture and this door merely routes it.
//
// ---- WHO MAY ASK ---------------------------------------------------------------
//
// AN OFFICE, AND ONLY AN OFFICE -- the arrangement door's rule and its exact honesty about
// what that is not. It names nobody, so a second tool asks with no edit here; it is NOT
// containment, because the loader binds `allow_any()` to every library it opens. What it
// buys is that every answer went to a named office, and -- for `RecipesDoor` -- that every
// recipe file this host wrote was written for one.
//
// ---- WHAT THEY CANNOT DO -------------------------------------------------------
//
// `ProjectDoor` answers two strings and can do nothing else, ever. `RecipesDoor` can install
// a catalog and append one authored recipe row, through the writer that already owns the
// recipe law, the atomic save and the one install seam -- and it can do nothing else: it
// mounts nothing, loads nothing, starts no process, and holds no `Session`. Neither
// publishes: every answer goes to the one weave that asked.

#include "builder_seam_vocabulary.hpp"
#include "pane_seam_vocabulary.hpp"
#include "weave.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace zengine::workshop {

/// WHAT THE PROJECT DOOR HAS DONE, and it is all counters -- `ArrangementDoorState`'s own
/// shape and its own reason: an answer kept between asks would be the mirror these doors
/// exist not to build. The three questions are counted apart because they are three
/// questions, and "how many times was this host asked what it is waiting on" is a different
/// number from "how many times was it asked where it began".
struct ProjectDoorState {
    std::int64_t answers = 0;   ///< where this run began, and where its marks live
    std::int64_t frontiers = 0; ///< what realization is waiting on
    std::int64_t lookups = 0;   ///< whether the plan in force names an artifact
    std::int64_t refused = 0;   ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(ProjectDoorState, 1, ZEN_FIELD(answers), ZEN_FIELD(frontiers),
              ZEN_FIELD(lookups), ZEN_FIELD(refused));
};

/// THE READ-ONLY PROJECT OFFICE: where this run began, what its realization is waiting on,
/// and whether its plan already names an artifact -- answered to whoever asks.
///
/// ⚠ THREE QUESTIONS, ONE OFFICE, AND NOT ONE OF THEM RUNS ANYBODY'S CODE. That is the
/// whole membership rule: an answer that changed the project would belong at an ACTING
/// office (`RecipesDoor`, `PlanDoor`), so "which office can write a maker's files" keeps a
/// one-word answer. The first is two strings the host captured once; the second and third
/// are closures the host already wired over owners it holds, spent at the moment of the ask.
class ProjectDoor
    : public loom::WeaveBase<
          ProjectDoor, ProjectDoorState,
          loom::Accept<ProjectRootRequested, ProjectFrontierRequested, PlanNamesRequested>,
          loom::Emit<ProjectRoot, ProjectFrontierSaid, PlanNames>> {
public:
    using Frontier = std::function<ProjectFrontier()>;
    using Names = std::function<bool(const std::string&)>;

    ProjectDoor(const std::string& project_dir, const std::string& marks_path,
                Frontier frontier = {}, Names names = {})
        : project_dir_(&project_dir), marks_path_(&marks_path), frontier_(std::move(frontier)),
          names_(std::move(names)) {}

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

    /// WHAT REALIZATION IS STOPPED ON, DERIVED AT THE ASK. The reading is the host's
    /// (`HostContext::frontier`, itself derived from the realization owner's own cursor at
    /// every spend), so no copy is taken anywhere on the path and the answer is the owner's
    /// frontier at this instant rather than at the instant somebody last refreshed one.
    ///
    /// A HOST THAT WIRED NONE ANSWERS THE DESIGNED ABSENCE. `waiting` false with an empty
    /// artifact is exactly what a project that is complete, still loading, or was never
    /// begun answers, so a host with no realization at all needs no second grammar.
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

private:
    const std::string* project_dir_;
    const std::string* marks_path_;
    Frontier frontier_;
    Names names_;
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

/// WHICH ARTIFACTS THIS PROJECT LOADS -- the one office that may change them (LOAD-IT).
///
/// ⚠ IT IS NOT `RecipesDoor`, AND THE TWO FILES ARE THE REASON. A recipe catalog says how an
/// artifact is produced; a load plan says which artifacts this project runs. Both are files a
/// maker authored and both are written through `workshop/authoring.hpp`'s writers, and one
/// office that could write either would be an office whose one act is two acts -- which is
/// the property the read/act split was drawn to keep.
///
/// IT HOLDS THE CLOSURE THE HOST ALREADY WIRED and spends it at the moment of the ask,
/// `RecipesDoor`'s shape exactly: the plan's law, the row's composition, the running
/// project's own answer, the project plan seeded from the plan read at launch, and the
/// atomic save are all `authoring::append_plan_row`'s, unchanged, behind one sentence. This
/// door mounts nothing, loads nothing, starts no process, holds no `Session`, and does not
/// publish: every answer goes to the one weave that asked.
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
