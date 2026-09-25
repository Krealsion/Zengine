// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP
#define ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP

// The build recipes' own file: which artifacts this project can build, and how.
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

/// What a Workshop build-recipe catalog says it is: its own word, so handing Workshop the wrong
/// file is named rather than half-read.
inline constexpr const char* kFormat = "zengine-build-recipes";

/// The build-recipe format version this build WRITES. It reads this one and version 1
/// (`v1` below), whose `cmake_target` rows carry no editing entry.
// WL-CODE-05 -- agents/workshop/code.md
inline constexpr std::int64_t kFormatVersion = 2;

/// THE CATALOG WORKSHOP SHIPS, by the name it is staged under BESIDE THE EXECUTABLE.
// WL-PROJ-15 -- agents/workshop/project.md
inline constexpr const char* kDefaultRecipesName = "default-build-recipes.json";

/// The catalog a maker authors into, under the project: where a chosen candidate's row goes when
/// the catalog in force is the shipped default or there is none. The shipped file is never
/// written into.
// WL-AUTH-01 -- agents/workshop/authoring.md
inline constexpr const char* kProjectRecipesName = "build-recipes.json";

/// Which catalog is in force at launch: `load_persist::plan_in_force`'s rule (an explicit
/// `--recipes`, else the project catalog, else the shipped default). The shipped default may be
/// absent, the ordinary "nothing to build", which the caller says with the same probe.
// WL-PROJ-15 -- agents/workshop/project.md
template <class Present>
inline std::string recipes_in_force(const std::string& explicit_path,
                                    const std::string& project_dir, const std::string& host_dir,
                                    Present present) {
    if (!explicit_path.empty()) {
        return explicit_path;
    }
    if (!project_dir.empty()) {
        const std::string project = project_dir + "/" + kProjectRecipesName;
        if (present(project)) {
            return project;
        }
    }
    return host_dir + "/" + kDefaultRecipesName;
}

/// The ceiling: comfortably above any real catalog, so a forged file does not choose the cost of
/// refusing it.
inline constexpr std::uintmax_t kMaxRecipeBytes = 1u << 16;

// ---- The file's own shapes -----------------------------------------------------

/// AN EXISTING CMAKE TARGET, AS WRITTEN -- and, since version 2, where editing its code
/// begins. `entry` is an ordinary string whose empty value is the absence: admission has no
/// optional, so a row that names no entry says `""`, exactly as `config` does.
struct WorkshopCMakeTarget {
    std::string build_dir;
    std::string target;
    std::string config;
    std::string entry;

    ZEN_SHAPE(WorkshopCMakeTarget, 2, ZEN_FIELD(build_dir), ZEN_FIELD(target),
              ZEN_FIELD(config), ZEN_FIELD(entry));
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

    ZEN_SHAPE(WorkshopRecipe, 2, ZEN_FIELD(recipe), ZEN_FIELD(artifact),
              ZEN_FIELD(artifact_dir), ZEN_FIELD(cmake_target), ZEN_FIELD(single_source));
};

/// A WHOLE SAVED RECIPE CATALOG.
struct WorkshopRecipeFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopRecipe> recipes;

    ZEN_SHAPE(WorkshopRecipeFile, 2, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(recipes));
};

// ---- Version 1, retained for reading -----------------------------------------------------
// A catalog is a file a maker named, with no session to ride, so the old shapes stay beside the
// reader (agents/decisions/setup-format-v3.md); a version-1 file reads as every row, no entry.
// WL-CODE-05 -- agents/workshop/code.md
namespace v1 {

struct WorkshopCMakeTarget {
    std::string build_dir;
    std::string target;
    std::string config;

    ZEN_SHAPE(WorkshopCMakeTarget, 1, ZEN_FIELD(build_dir), ZEN_FIELD(target),
              ZEN_FIELD(config));
};

struct WorkshopRecipe {
    std::string recipe;
    std::string artifact;
    std::string artifact_dir;
    std::vector<WorkshopCMakeTarget> cmake_target;
    std::vector<WorkshopSingleSource> single_source;

    ZEN_SHAPE(WorkshopRecipe, 1, ZEN_FIELD(recipe), ZEN_FIELD(artifact),
              ZEN_FIELD(artifact_dir), ZEN_FIELD(cmake_target), ZEN_FIELD(single_source));
};

struct WorkshopRecipeFile {
    std::string format;
    std::int64_t format_version = 0;
    std::vector<WorkshopRecipe> recipes;

    ZEN_SHAPE(WorkshopRecipeFile, 1, ZEN_FIELD(format), ZEN_FIELD(format_version),
              ZEN_FIELD(recipes));
};

/// A version-1 catalog as the current shapes say it: the same rows, and no entry anywhere.
/// The format claim is carried as it was written, so the checks after admission judge the
/// file's own words and not a translation's.
inline recipe_persist::WorkshopRecipeFile current(const WorkshopRecipeFile& old) {
    recipe_persist::WorkshopRecipeFile now;
    now.format = old.format;
    now.format_version = old.format_version;
    now.recipes.reserve(old.recipes.size());
    for (const WorkshopRecipe& row : old.recipes) {
        recipe_persist::WorkshopRecipe converted;
        converted.recipe = row.recipe;
        converted.artifact = row.artifact;
        converted.artifact_dir = row.artifact_dir;
        for (const WorkshopCMakeTarget& t : row.cmake_target) {
            converted.cmake_target.push_back(
                recipe_persist::WorkshopCMakeTarget{t.build_dir, t.target, t.config, std::string()});
        }
        converted.single_source = row.single_source;
        now.recipes.push_back(std::move(converted));
    }
    return now;
}

} // namespace v1

/// The envelope's shape version and the catalog format version are one number, so a file from
/// another version is refused by its number before a row is judged.
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
            row.cmake_target.push_back(WorkshopCMakeTarget{r.cmake_target->build_dir,
                                                           r.cmake_target->target,
                                                           r.cmake_target->config,
                                                           r.cmake_target->entry});
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
    return "build recipes version " + std::to_string(found) + " -- this Workshop reads versions " +
           std::to_string(v1::WorkshopRecipeFile::zen_version) + " and " +
           std::to_string(kFormatVersion);
}

/// The file grammar's one law beyond the recipe's: a list can break it, the typed recipe cannot.
inline Written check_mechanism_counts(const WorkshopRecipe& row) {
    if (row.cmake_target.size() > 1) {
        return Written::no("recipe `" + row.recipe + "` declares a CMake target more than once");
    }
    if (row.single_source.size() > 1) {
        return Written::no("recipe `" + row.recipe + "` declares a source file more than once");
    }
    return Written::ok();
}

/// Text to a catalog; total. The version is answered from the claim before any row is judged:
/// version 1 against its own retained shapes, read as the current rows with no entry; any other
/// version refused by its number.
// WL-PROJ-04 -- agents/workshop/project.md; WL-CODE-05 -- agents/workshop/code.md
inline LoadedRecipes from_text(std::string_view bytes) {
    const loom::Unverified claim = loom::compat::parse(bytes);
    if (!claim.well_formed()) {
        const loom::Admission refused =
            loom::admit(claim, loom::schema_of<WorkshopRecipeFile>(), loom::Report::FirstError);
        return LoadedRecipes::no("not a Workshop build-recipe catalog: " +
                                 refused.first_error().message());
    }
    const bool named = claim.claimed_name() == std::string(WorkshopRecipeFile::zen_name);
    WorkshopRecipeFile file;
    std::int64_t claimed = kFormatVersion;
    if (named && claim.claimed_version() == v1::WorkshopRecipeFile::zen_version) {
        const loom::Admission old =
            loom::admit(claim, loom::schema_of<v1::WorkshopRecipeFile>(), loom::Report::FirstError);
        if (!old.ok()) {
            return LoadedRecipes::no(old.first_error().message());
        }
        file = v1::current(loom::from_value<v1::WorkshopRecipeFile>(old.value()));
        claimed = static_cast<std::int64_t>(v1::WorkshopRecipeFile::zen_version);
    } else {
        if (named && claim.claimed_version() != WorkshopRecipeFile::zen_version) {
            return LoadedRecipes::no(
                wrong_version(static_cast<std::int64_t>(claim.claimed_version())));
        }
        const loom::Admission admitted =
            loom::admit(claim, loom::schema_of<WorkshopRecipeFile>(), loom::Report::FirstError);
        if (!admitted.ok()) {
            return LoadedRecipes::no(admitted.first_error().message());
        }
        file = loom::from_value<WorkshopRecipeFile>(admitted.value());
    }
    if (file.format != kFormat) {
        return LoadedRecipes::no("not a Workshop build-recipe catalog: it says it is `" +
                                 file.format + "`");
    }
    // THE FORMAT FIELD AGREES WITH THE ENVELOPE IT ARRIVED IN, or the file is refused naming
    // both numbers: an envelope of one version and a field of another is not a catalog either
    // reader was written for, and neither number alone says what is wrong with it.
    if (file.format_version != claimed) {
        return LoadedRecipes::no("build recipes say version " +
                                 std::to_string(file.format_version) +
                                 " inside a version-" + std::to_string(claimed) +
                                 " envelope -- a catalog is one version");
    }

    std::vector<builder::Recipe> candidate;
    candidate.reserve(file.recipes.size());
    for (const WorkshopRecipe& row : file.recipes) {
        const Written counted = check_mechanism_counts(row);
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
                                                        row.cmake_target.front().config,
                                                        row.cmake_target.front().entry};
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
inline Written save_file(const std::string& path, const std::vector<builder::Recipe>& recipes) {
    return persist::write_file(path, to_text(recipes));
}

/// Fill in the facts an authored recipe cannot carry, and only those. A single-source build lands
/// in its own workspace (`<workspace>/out`), never on the loaded file: rebuilding a mapped image
/// fails on Windows and changes running code on Linux (why:
/// agents/decisions/a-reload-lands-off-the-loaded-path.md). A `cmake_target`'s empty
/// `artifact_dir` means beside the host; a relative editing entry completes against the project,
/// an absolute one stays as written, and an empty one stays empty.
// WL-PROJ-02, WL-PROJ-16 -- agents/workshop/project.md
// WL-CODE-05 -- agents/workshop/code.md
inline void complete_recipes(std::vector<builder::Recipe>& recipes, const std::string& host_dir,
                             const std::string& project_dir) {
    for (builder::Recipe& r : recipes) {
        if (r.single_source.has_value()) {
            if (r.single_source->workspace.empty()) {
                r.single_source->workspace = host_dir + "/build-workspace/" + r.id;
            }
            if (r.artifact_dir.empty()) {
                r.artifact_dir = r.single_source->workspace + "/out";
            }
            r.single_source->source =
                persist::resolved_against(project_dir, r.single_source->source);
        }
        if (r.cmake_target.has_value() && !r.cmake_target->entry.empty()) {
            r.cmake_target->entry = persist::resolved_against(project_dir, r.cmake_target->entry);
        }
        if (r.artifact_dir.empty()) {
            r.artifact_dir = host_dir;
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
