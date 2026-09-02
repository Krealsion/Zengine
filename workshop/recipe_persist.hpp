// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP
#define ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP

// THE BUILD RECIPES' OWN FILE -- the fourth durable artifact beside the
// document's, the setup's and the load plan's.
// Workshop law: agents/workshop/project.md

#include "persist.hpp"

#include "builder/recipe.hpp"

#include <zen/admission.hpp>
#include <zen/schema.hpp>
#include <zen/serialize.hpp>
#include <zen/value.hpp>
#include <zen/weave/shape.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace zengine::workshop::recipe_persist {

namespace builder = zengine::builder;

/// What a Workshop build-recipe catalog says it is. Its own word, beside and not equal
/// to the document's, the setup's or the plan's, so that handing Workshop the wrong one
/// of its four files is named rather than half-read.
inline constexpr const char* kFormat = "zengine-build-recipes";

/// The only build-recipe format version this build reads or writes.
inline constexpr std::int64_t kFormatVersion = 1;

/// THE CATALOG WORKSHOP SHIPS, by the name it is staged under BESIDE THE EXECUTABLE.
// WL-PROJ-02 -- agents/workshop/project.md
inline constexpr const char* kDefaultRecipesName = "default-build-recipes.json";

/// A recipe catalog is small, and its ceiling says so. Thirty-two recipes of a name, an
/// artifact, two paths, eight prefixes and sixteen link targets is comfortably under
/// this; what it bounds is a forged file, which does not get to choose the cost of
/// refusing it.
inline constexpr std::uintmax_t kMaxRecipeBytes = 1u << 16;

// ---- The file's own shapes -----------------------------------------------------

/// AN EXISTING CMAKE TARGET, AS WRITTEN.
struct WorkshopCMakeTarget {
    std::string build_dir;
    std::string target;
    std::string config;

    ZEN_SHAPE(WorkshopCMakeTarget, 1, ZEN_FIELD(build_dir), ZEN_FIELD(target),
              ZEN_FIELD(config));
};

/// ONE SOURCE FILE, AS WRITTEN.
struct WorkshopSingleSource {
    std::string source;
    std::vector<std::string> packages;
    std::vector<std::string> links;
    std::string toolchain_from;
    std::string workspace;

    ZEN_SHAPE(WorkshopSingleSource, 1, ZEN_FIELD(source), ZEN_FIELD(packages),
              ZEN_FIELD(links), ZEN_FIELD(toolchain_from), ZEN_FIELD(workspace));
};

/// ONE RECIPE ROW AS WRITTEN: what it is called, what it makes, where that lands, and
/// which of the two mechanisms makes it.
struct WorkshopRecipe {
    std::string recipe;
    std::string artifact;
    std::string artifact_dir;
    std::vector<WorkshopCMakeTarget> cmake_target;
    std::vector<WorkshopSingleSource> single_source;

    ZEN_SHAPE(WorkshopRecipe, 1, ZEN_FIELD(recipe), ZEN_FIELD(artifact),
              ZEN_FIELD(artifact_dir), ZEN_FIELD(cmake_target), ZEN_FIELD(single_source));
};

/// A WHOLE SAVED RECIPE CATALOG.
struct WorkshopRecipeFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopRecipe> recipes;

    ZEN_SHAPE(WorkshopRecipeFile, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(recipes));
};

/// THE ENVELOPE'S SHAPE VERSION AND THE CATALOG FORMAT VERSION ARE ONE NUMBER.
/// `load_persist.hpp`'s decision, for its reason: there is no history in which the two
/// could sensibly disagree, and coupling them is what lets a file from another version
/// be refused by ITS NUMBER before a single row is judged against this version's shape.
static_assert(WorkshopRecipeFile::zen_version == static_cast<std::uint32_t>(kFormatVersion),
              "the recipe catalog's format version and its envelope's shape version are one "
              "number: a file from another version must be refused by ITS NUMBER, before "
              "its rows are judged against this version's shape");

// ---- Writing --------------------------------------------------------------------

inline WorkshopRecipeFile to_file(const std::vector<builder::Recipe>& recipes) {
    WorkshopRecipeFile out;
    out.format = kFormat;
    out.format_version = kFormatVersion;
    out.recipes.reserve(recipes.size());
    for (const builder::Recipe& r : recipes) {
        // AS AUTHORED. Not sorted, not resolved against a disk, not dropped for naming
        // a source file this machine does not have. `artifact_dir` is written EXACTLY
        // as it was read, empty included -- the host's substitution of its own artifact
        // directory happens after this file is gone, and writing the substitution back
        // would turn a portable catalog into one machine's.
        WorkshopRecipe row;
        row.recipe = r.id;
        row.artifact = r.artifact;
        row.artifact_dir = r.artifact_dir;
        if (r.cmake_target.has_value()) {
            row.cmake_target.push_back(WorkshopCMakeTarget{
                r.cmake_target->build_dir, r.cmake_target->target, r.cmake_target->config});
        }
        if (r.single_source.has_value()) {
            row.single_source.push_back(WorkshopSingleSource{
                r.single_source->source, r.single_source->packages, r.single_source->links,
                r.single_source->toolchain_from, r.single_source->workspace});
        }
        out.recipes.push_back(std::move(row));
    }
    return out;
}

inline std::string to_text(const std::vector<builder::Recipe>& recipes) {
    return loom::compat::serialize(loom::to_value(to_file(recipes)));
}

// ---- Reading ---------------------------------------------------------------------

/// What reading produced: whether it worked, and the catalog if it did.
struct LoadedRecipes {
    Written outcome;
    std::vector<builder::Recipe> recipes;

    static LoadedRecipes no(std::string why) {
        return LoadedRecipes{Written::no(std::move(why)), {}};
    }
};

/// WHAT TO SAY ABOUT A CATALOG VERSION THIS BUILD DOES NOT READ. One sentence, one
/// place, so the two doors that can meet a wrong version cannot word it differently.
inline std::string wrong_version(std::int64_t found) {
    return "build recipes version " + std::to_string(found) +
           " -- this Workshop reads version " + std::to_string(kFormatVersion);
}

/// EVERY LAW THE FILE'S OWN GRAMMAR ADDS on top of the recipe law -- which is exactly
/// the one question the typed recipe cannot ask, because the typed recipe has already
/// answered it by construction: how MANY mechanisms a row carries.
inline Written check_recipe_file(const WorkshopRecipe& row) {
    if (row.cmake_target.size() > 1) {
        return Written::no("recipe `" + row.recipe + "` declares a CMake target more than once");
    }
    if (row.single_source.size() > 1) {
        return Written::no("recipe `" + row.recipe + "` declares a source file more than once");
    }
    return Written::ok();
}

/// Text to a catalog. Total: every input is either a catalog or a refusal with a
/// reason, and nothing here throws.
// WL-PROJ-04 -- agents/workshop/project.md
inline LoadedRecipes from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopRecipeFile>(), loom::Report::FirstError);
        return LoadedRecipes::no("not a Workshop build-recipe catalog: " +
                                 refused.first_error().message());
    }
    if (claim.claimed_name() == std::string(WorkshopRecipeFile::zen_name) &&
        claim.claimed_version() != WorkshopRecipeFile::zen_version) {
        return LoadedRecipes::no(
            wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
    }
    const loom::Admission admitted =
        loom::admit(claim, loom::schema_of<WorkshopRecipeFile>(), loom::Report::FirstError);
    if (!admitted.ok()) {
        return LoadedRecipes::no(admitted.first_error().message());
    }

    const WorkshopRecipeFile file = loom::from_value<WorkshopRecipeFile>(admitted.value());
    if (file.format != kFormat) {
        return LoadedRecipes::no("not a Workshop build-recipe catalog: it says it is `" +
                                 file.format + "`");
    }
    if (file.format_version != kFormatVersion) {
        return LoadedRecipes::no(wrong_version(file.format_version));
    }

    std::vector<builder::Recipe> candidate;
    candidate.reserve(file.recipes.size());
    for (const WorkshopRecipe& row : file.recipes) {
        const Written counted = check_recipe_file(row);
        if (!counted.accepted) {
            return LoadedRecipes::no(counted.refusal);
        }
        builder::Recipe r;
        r.id = row.recipe;
        r.artifact = row.artifact;
        r.artifact_dir = row.artifact_dir;
        if (!row.cmake_target.empty()) {
            r.cmake_target = builder::CMakeTargetRecipe{row.cmake_target.front().build_dir,
                                                        row.cmake_target.front().target,
                                                        row.cmake_target.front().config};
        }
        if (!row.single_source.empty()) {
            r.single_source = builder::SingleSourceRecipe{
                row.single_source.front().source, row.single_source.front().packages,
                row.single_source.front().links, row.single_source.front().toolchain_from,
                row.single_source.front().workspace};
        }
        candidate.push_back(std::move(r));
    }
    const std::string legal = builder::check_recipes(candidate);
    if (!legal.empty()) {
        return LoadedRecipes::no(legal);
    }

    LoadedRecipes loaded;
    loaded.outcome = Written::ok();
    loaded.recipes = std::move(candidate);
    return loaded;
}

// ---- The file itself ---------------------------------------------------------------

/// Save a catalog to a file, through the document's own safe write.
// WL-PROJ-02 -- agents/workshop/project.md
inline Written save_file(const std::string& path, const std::vector<builder::Recipe>& recipes) {
    return persist::write_file(path, to_text(recipes));
}

/// FILL IN THE FACTS AN AUTHORED RECIPE CANNOT CARRY, and only those.
// WL-PROJ-02 -- agents/workshop/project.md
inline void complete_recipes(std::vector<builder::Recipe>& recipes, const std::string& host_dir,
                             const std::string& project_dir) {
    for (builder::Recipe& r : recipes) {
        if (r.artifact_dir.empty()) {
            r.artifact_dir = host_dir;
        }
        if (r.single_source.has_value()) {
            if (r.single_source->workspace.empty()) {
                r.single_source->workspace = host_dir + "/build-workspace/" + r.id;
            }
            r.single_source->source =
                persist::resolved_against(project_dir, r.single_source->source);
        }
    }
}

/// Read a catalog from a file. The composition of every layer: the file, the format,
/// and the recipe law.
inline LoadedRecipes load_file(const std::string& path) {
    const persist::FileText read =
        persist::read_file(path, kMaxRecipeBytes, "a Workshop build-recipe catalog");
    if (!read.outcome.accepted) {
        return LoadedRecipes{read.outcome, {}};
    }
    return from_text(read.text);
}

} // namespace zengine::workshop::recipe_persist

#endif // ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP
