// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_AUTHORING_HPP
#define ZENGINE_WORKSHOP_AUTHORING_HPP

// THE TWO AUTHORED FILES GAIN A WRITER, AND IT IS THE MAKER'S OWN ACT (PICK-1, LOAD-IT).
//
// A recipe row and a plan row are written here and nowhere else: composed from what the
// maker typed, checked by the file's own law, appended to the file's rows AS WRITTEN --
// never the completed ones the session holds -- saved through the sibling-and-rename every
// durable file uses, and then made current through the one seam a chosen catalog already
// spends. Nothing here completes a host path back into a file, discovers a recipe, or
// rewrites a row a maker did not ask for.
//
// WHICH FILE (Choice 5). A recipe goes into the catalog in force when the maker named or
// authored that catalog. When the catalog in force is the shipped default beside the
// executable -- installation truth -- or there is none, the row goes into a PROJECT
// catalog, `<project>/build-recipes.json`, seeded with the rows in force as written, and
// that file is installed. A plan row goes into the project plan, `<project>/workshop-plan.json`,
// seeded from the plan in force as read at launch (decision 3c); the launch rule in
// `load_persist::plan_in_force` is what makes that file the plan next time.
//
// THE RUNNING PROJECT FIRST, THEN THE FILE. The executor's `append` is asked before the
// plan file is written, so a row the running project refuses is written nowhere; a row it
// took whose file could not be written is said in both halves.
//
// ...AND THEN THE PRODUCT, WHEN THERE IS ONE (decision 3). A row that became the frontier
// whose recipe has already built its product -- a plain `b` left it in its workspace -- is
// one key from loaded, and `load it` should end loaded. The writer says where the product
// is, through the staging rule that owns that question, and the weave performs the button's
// own act with it; nothing here builds, stages or loads.
// Workshop law: agents/workshop/authoring.md

#include "load_execute.hpp"
#include "load_persist.hpp"
#include "persist.hpp"
#include "recipe_persist.hpp"
#include "recipes.hpp"
#include "staging.hpp"
#include "weave.hpp"

#include "builder/recipe.hpp"

#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace zengine::workshop::authoring {

/// WHAT THE RECIPE WRITER SPENDS: the host's two directories, the catalog owner (read at
/// the act, never copied) and the one seam that installs a file.
struct RecipeAuthor {
    std::string host_dir;
    std::string project_dir;
    const CurrentRecipes* recipes = nullptr;
    std::function<HostContext::RecipeSwap(const std::string&)> use_recipes;
};

/// COMPOSE A RECIPE FROM THE DRAFT. Strings in, a typed row out; the law is applied by
/// the caller.
inline builder::Recipe compose(const HostContext::RecipeDraft& draft) {
    builder::Recipe r;
    r.id = draft.id;
    r.artifact = draft.artifact;
    if (draft.tree) {
        r.artifact_dir = draft.artifact_dir;
        r.cmake_target = builder::CMakeTargetRecipe{draft.build_dir, draft.target, std::string()};
    } else {
        builder::SingleSourceRecipe one;
        one.source = draft.source;
        one.packages = draft.packages;
        one.links = draft.links;
        r.single_source = one;
    }
    return r;
}

/// THE FILE A NEW ROW GOES INTO, and the rows it starts from (as written).
struct Target {
    std::string path;
    std::vector<builder::Recipe> rows;
    std::string refusal;
};

inline Target recipe_target(const RecipeAuthor& host) {
    Target out;
    const std::string in_force = host.recipes != nullptr ? host.recipes->source() : std::string();
    const std::string shipped = host.host_dir + "/" + recipe_persist::kDefaultRecipesName;
    const bool own = !in_force.empty() && in_force != shipped;
    if (own) {
        out.path = in_force;
        const recipe_persist::LoadedRecipes read = recipe_persist::load_file(out.path);
        if (!read.outcome.accepted) {
            out.refusal = "the catalog in force could not be read back: " + read.outcome.refusal;
            return out;
        }
        out.rows = read.recipes;
        return out;
    }
    if (host.project_dir.empty()) {
        out.refusal = "this run began nowhere -- a recipe needs a project to be written into";
        return out;
    }
    out.path = host.project_dir + "/" + recipe_persist::kProjectRecipesName;
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(out.path), ec) && !ec) {
        // A PROJECT CATALOG ALREADY THERE is the file the row goes into, whatever is in
        // force: it is the maker's, and seeding over it would drop their rows.
        const recipe_persist::LoadedRecipes read = recipe_persist::load_file(out.path);
        if (!read.outcome.accepted) {
            out.refusal = out.path + " could not be read back: " + read.outcome.refusal;
            return out;
        }
        out.rows = read.recipes;
        return out;
    }
    if (!in_force.empty()) {
        // SEEDED WITH THE SHIPPED ROWS AS WRITTEN, so nothing buildable disappears from the
        // panel at the moment a maker authors their first recipe (Choice 10).
        const recipe_persist::LoadedRecipes read = recipe_persist::load_file(in_force);
        if (read.outcome.accepted) {
            out.rows = read.recipes;
        }
    }
    return out;
}

/// AUTHOR ONE RECIPE ROW: compose, check, choose the file, read its rows as written,
/// append, check the whole, save atomically, install through the one seam.
// WL-AUTH-01 -- agents/workshop/authoring.md
inline HostContext::RecipeSwap author_recipe(const RecipeAuthor& host,
                                             const HostContext::RecipeDraft& draft) {
    HostContext::RecipeSwap out;
    out.path = host.recipes != nullptr ? host.recipes->source() : std::string();
    out.recipes = host.recipes != nullptr ? host.recipes->all().size() : 0;
    const builder::Recipe row = compose(draft);
    const std::string law = builder::check_recipe(row);
    if (!law.empty()) {
        out.refusal = law;
        return out;
    }
    Target target = recipe_target(host);
    if (!target.refusal.empty()) {
        out.refusal = target.refusal;
        return out;
    }
    target.rows.push_back(row);
    const std::string whole = builder::check_recipes(target.rows);
    if (!whole.empty()) {
        out.refusal = whole;
        return out;
    }
    const Written saved = recipe_persist::save_file(target.path, target.rows);
    if (!saved.accepted) {
        out.refusal = saved.refusal;
        return out;
    }
    if (!host.use_recipes) {
        out.refusal = "written to " + target.path + ", but this host cannot install it";
        return out;
    }
    return host.use_recipes(target.path);
}

/// WHAT THE PLAN WRITER SPENDS: the project plan's path (empty with no project), the plan
/// in force as read at launch and appended since, the running project's owner, and the
/// host's staging rule -- read for where a product is, never for an act.
struct PlanAuthor {
    std::string path;
    load::LoadPlan rows;
    load::PlanExecutor* executor = nullptr;
    const staging::Host* staging = nullptr;
};

inline bool plan_names(const PlanAuthor& plan, const std::string& stem) {
    for (const load::ArtifactIntent& a : plan.rows.artifacts) {
        if (a.stem == stem) {
            return true;
        }
    }
    return false;
}

/// AUTHOR THE MINIMUM PLAN ROW: the running project first, then the file, then where the
/// chosen recipe's product is when the row is the frontier and the product exists.
// WL-AUTH-02, WL-AUTH-03 -- agents/workshop/authoring.md
inline HostContext::PlanAppend append_plan_row(PlanAuthor& plan, const std::string& stem,
                                               const std::string& role,
                                               const std::string& recipe) {
    HostContext::PlanAppend out;
    load::ArtifactIntent row;
    row.stem = stem;
    row.weave = load::WeaveIntent{role};
    const Written law = load::check_artifact(row);
    if (!law.accepted) {
        out.refusal = law.refusal;
        return out;
    }
    load::LoadPlan candidate = plan.rows;
    candidate.artifacts.push_back(row);
    const Written whole = load::check_plan(candidate);
    if (!whole.accepted) {
        out.refusal = whole.refusal;
        return out;
    }
    if (plan.path.empty()) {
        out.refusal = "this run began nowhere -- a plan row needs a project to be written into";
        return out;
    }
    if (plan.executor == nullptr) {
        out.refusal = "this host has no realization owner to append to";
        return out;
    }
    const load::PlanExecutor::Appended taken = plan.executor->append(row);
    if (!taken.accepted) {
        out.refusal = taken.refusal;
        return out;
    }
    plan.rows = std::move(candidate);
    out.accepted = true;
    out.detail = taken.detail;
    // THE BRANCH `o` FINISHES ON: the new row is the frontier -- derived from the owner's
    // cursor, never from the detail's words -- and its recipe's product is on disk. Behind
    // another frontier, resolved already, or with nothing built yet, there is nothing to
    // finish, and `product` stays empty.
    out.frontier = plan.executor->waiting_on() == stem;
    if (out.frontier && plan.staging != nullptr) {
        out.product = staging::product_of(*plan.staging, stem, recipe);
    }
    const Written saved = load_persist::save_file(plan.path, plan.rows);
    if (!saved.accepted) {
        out.detail += "; the plan file was NOT written: " + saved.refusal;
        return out;
    }
    out.path = plan.path;
    return out;
}

} // namespace zengine::workshop::authoring

#endif // ZENGINE_WORKSHOP_AUTHORING_HPP
