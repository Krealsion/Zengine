// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_RECIPES_HPP
#define ZENGINE_WORKSHOP_RECIPES_HPP

// THE RECIPES THIS RUNNING WORKSHOP CURRENTLY MEANS, AND WHICH FILE THEY CAME FROM
// Workshop law: agents/workshop/project.md

#include "recipe_persist.hpp"

#include "builder/recipe.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// The one current completed recipe catalog of a running Workshop, and the authored file
/// it came from.
class CurrentRecipes {
public:
    /// HOW THIS PLATFORM SPELLS AN ARTIFACT STEM AS A FILE, in a named directory.
    ///
    /// This class cannot know it and must not guess it: the load plan makes that rule the
    /// HOST's, written in exactly one place, which is what keeps one authored plan legal
    /// on Linux and on Windows. So the host hands it over -- one function, no state --
    /// exactly as it hands `complete_recipes` its two directories.
    using ArtifactFile = std::string (*)(std::string_view directory, std::string_view stem);

    /// TAKE CUSTODY OF A COMPLETED CATALOG AND OF THE FILE IT CAME FROM.
    // WL-PROJ-03, WL-PROJ-04 -- agents/workshop/project.md
    void hold(std::string source, std::vector<builder::Recipe> completed,
              ArtifactFile artifact_file) {
        source_ = std::move(source);
        all_ = std::move(completed);
        views_.clear();
        views_.reserve(all_.size());
        for (const builder::Recipe& r : all_) {
            views_.push_back(
                builder::RecipeView{r.id, r.artifact, artifact_file(r.artifact_dir, r.artifact)});
        }
    }

    /// THE AUTHORED FILE THESE RECIPES CAME FROM -- provenance, and never content.
    /// Empty means no catalog is in force, which is an ordinary state: a project with
    /// nothing to build is a project.
    const std::string& source() const noexcept { return source_; }

    /// THE WHOLE COMPLETED CATALOG, for the parties that carry out a build or answer for
    /// a recipe's own inputs -- the runner, and the host's edit-source answer.
    const std::vector<builder::Recipe>& all() const noexcept { return all_; }

    /// THE SAME CATALOG WITH THE BUILD PROCEDURE SUBTRACTED, for the presentation side.
    /// One row per recipe, in the catalog's own order, so an index into one is an index
    /// into the other.
    const std::vector<builder::RecipeView>& views() const noexcept { return views_; }

private:
    std::string source_;
    std::vector<builder::Recipe> all_;
    std::vector<builder::RecipeView> views_;
};

/// READ AN AUTHORED CATALOG FILE AND MAKE IT THIS SESSION'S -- the ONE seam, spent by the
/// launch and by every later maker choice alike.
// WL-PROJ-02, WL-PROJ-04 -- agents/workshop/project.md
inline Written install_recipes(CurrentRecipes& owner, const std::string& path,
                               const std::string& host_dir, const std::string& project_dir,
                               CurrentRecipes::ArtifactFile artifact_file) {
    recipe_persist::LoadedRecipes read = recipe_persist::load_file(path);
    if (!read.outcome.accepted) {
        return read.outcome; // NOTHING WAS TOUCHED: the candidate never became a catalog
    }
    recipe_persist::complete_recipes(read.recipes, host_dir, project_dir);
    owner.hold(path, std::move(read.recipes), artifact_file);
    return Written::ok();
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_RECIPES_HPP
