// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_RECIPES_HPP
#define ZENGINE_WORKSHOP_RECIPES_HPP

// THE RECIPES THIS RUNNING WORKSHOP CURRENTLY MEANS (PROJ-0).
//
// ---- The two facts, and which one lives here ------------------------------------
//
//     authored recipe catalog        a durable FILE a maker wrote and can carry
//             +
//     host/project completion        where this install puts artifacts, where it may
//                                    generate a project, and what a relative source
//                                    is relative to (`recipe_persist::complete_recipes`)
//             |
//             v
//     COMPLETED SESSION RECIPES      what this process is actually going to build,
//                                    edit and look for -- and what this class holds
//
// The FILE stays authored truth and is never rewritten. What this owns is the OUTPUT of
// the one completion law, for as long as this Workshop is running.
//
// ---- Why it is a thing and not a local variable ---------------------------------
//
// It was a local variable, and the local was scattered: the runner took a whole COPY of
// the completed catalog, the tool took a copy of its reduced view, and the host's
// edit-source answer was a closure over a THIRD copy of three fields per recipe. After
// construction nobody held the sentence "these are the recipes this Workshop currently
// means" -- so the completed catalog could not be replaced without finding every
// consumer and re-synchronising it by hand, and the first consumer somebody forgot
// would go on answering from the old project in a way no test could see.
//
// So this holds it, every consumer READS it, and a replacement has ONE place to happen.
//
// ---- What this deliberately is NOT ----------------------------------------------
//
// It is not a live-switching mechanism, and PROJ-0 authorises none: nothing here decides
// what a replacement does to a chosen Builder row, an in-flight build, or a maker's
// standing pick, because those are the questions of the phase that adds the first real
// catalog-change gesture. What is settled here is only custody.
//
// It is not an authorship surface. There is no non-const access to what is held: a
// consumer may read the catalog and may not edit it, because a recipe a consumer wrote
// into memory would be a build procedure with no author and no file.
//
// It is not the Builder's, the Editor's, the browser's or a pane's, and it is not a
// build invocation's. Every one of those is a consumer with a shorter life than the
// answer, which is exactly why the answer needed an owner above all of them.
//
// ---- The lifetime rule, which is the whole safety argument ----------------------
//
// CONSUMERS HOLD REFERENCES INTO THIS OBJECT, so this object must outlive them. The host
// declares one BEFORE the HostContext, the bus, the Kernel and every weave, which makes
// reverse-destruction order the proof -- it is the same argument, one file over, that
// lets `HostContext::frontier` read a `PlanExecutor` the host also owns as a local.
//
// A REPLACEMENT REPLACES CONTENTS, NEVER THE OBJECT. `hold()` assigns into the two
// vectors this class already owns, so every reference a consumer took at construction
// keeps naming the live answer. What a consumer must not do is keep a pointer to one
// ROW across a turn -- `recipe_named`/`view_named` hand back element pointers, and those
// are for spending inside the operation that asked, exactly as they always were.

#include "builder/recipe.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The one current completed recipe catalog of a running Workshop.
class CurrentRecipes {
public:
    /// HOW THIS PLATFORM SPELLS AN ARTIFACT STEM AS A FILE, in a named directory.
    ///
    /// This class cannot know it and must not guess it: LOAD-0 makes that rule the
    /// HOST's, written in exactly one place, which is what keeps one authored plan legal
    /// on Linux and on Windows. So the host hands it over -- one function, no state --
    /// exactly as it hands `complete_recipes` its two directories.
    using ArtifactFile = std::string (*)(std::string_view directory, std::string_view stem);

    /// TAKE CUSTODY OF A COMPLETED CATALOG.
    ///
    /// `completed` has already been through the one completion law; this does not
    /// re-complete it, because a second party resolving an authored spelling is the
    /// defect that law exists to prevent. What this adds is the tool's REDUCED VIEW of
    /// each row, derived here so the two can never disagree: the Builder tool is
    /// deliberately told an identity, an artifact and the one file that artifact means,
    /// and never a source path, a build tree, a package prefix or a link list (BLD-1).
    ///
    /// CALLING IT AGAIN REPLACES WHAT IS HELD, and that is the seam a later phase spends:
    /// parse, complete, hold. Every consumer's reference survives it because the vectors
    /// below are members and only their CONTENTS change.
    void hold(std::vector<builder::Recipe> completed, ArtifactFile artifact_file) {
        all_ = std::move(completed);
        views_.clear();
        views_.reserve(all_.size());
        for (const builder::Recipe& r : all_) {
            views_.push_back(
                builder::RecipeView{r.id, r.artifact, artifact_file(r.artifact_dir, r.artifact)});
        }
    }

    /// THE WHOLE COMPLETED CATALOG, for the parties that carry out a build or answer for
    /// a recipe's own inputs -- the runner, and the host's edit-source answer.
    const std::vector<builder::Recipe>& all() const noexcept { return all_; }

    /// THE SAME CATALOG WITH THE BUILD PROCEDURE SUBTRACTED, for the presentation side.
    /// One row per recipe, in the catalog's own order, so an index into one is an index
    /// into the other.
    const std::vector<builder::RecipeView>& views() const noexcept { return views_; }

private:
    std::vector<builder::Recipe> all_;
    std::vector<builder::RecipeView> views_;
};

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_RECIPES_HPP
