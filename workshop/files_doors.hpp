// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_FILES_DOORS_HPP
#define ZENGINE_WORKSHOP_FILES_DOORS_HPP

// THE TWO DOORS THE FILES WEAVE ASKS, HELD BY THE HOST -- one that reads and one that acts.
//
//     ProjectDoor   zengine.project   ProjectRootRequested -> ProjectRoot
//     RecipesDoor   zengine.recipes   RecipeUseRequested    -> RecipeOutcome
//                                     RecipeAuthorRequested -> RecipeOutcome
//
// The third door -- one source opened in the Editor -- is not here: Workshop's own weave
// holds the Editor, so `OpenSourceRequested` is answered where `open_source` lives
// (`weave_pane_editor.cpp`), at the office Workshop already holds.
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

#include "files_seam_vocabulary.hpp"
#include "weave.hpp"

#include <zen/weave.hpp>

#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace zengine::workshop {

/// WHAT THE PROJECT DOOR HAS DONE, and it is all counters -- `ArrangementDoorState`'s own
/// shape and its own reason: an answer kept between asks would be the mirror these doors
/// exist not to build.
struct ProjectDoorState {
    std::int64_t answers = 0;
    std::int64_t refused = 0; ///< asks that were not authored as any office
    ZEN_EXPOSE();
    ZEN_SHAPE(ProjectDoorState, 1, ZEN_FIELD(answers), ZEN_FIELD(refused));
};

/// WHERE THIS RUN BEGAN AND WHERE ITS MARKS LIVE, ANSWERED TO WHOEVER ASKS.
class ProjectDoor : public loom::WeaveBase<ProjectDoor, ProjectDoorState,
                                           loom::Accept<ProjectRootRequested>,
                                           loom::Emit<ProjectRoot>> {
public:
    ProjectDoor(const std::string& project_dir, const std::string& marks_path)
        : project_dir_(&project_dir), marks_path_(&marks_path) {}

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

private:
    const std::string* project_dir_;
    const std::string* marks_path_;
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

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_FILES_DOORS_HPP
