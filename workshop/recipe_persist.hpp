// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP
#define ZENGINE_WORKSHOP_RECIPE_PERSIST_HPP

// THE BUILD RECIPES' OWN FILE (BLD-1) -- the fourth durable artifact beside the
// document's, the setup's and the load plan's, and separate from all three for the
// reason they are separate from each other (WS-0):
//
//   a DOCUMENT    is what a maker made
//   a SETUP       is the arrangement of panes they were looking at while they made it
//   a LOAD PLAN   is which artifacts this project RUNS ON
//   BUILD RECIPES are how artifacts this project cares about can be PRODUCED
//
// ---- Why it is not a section of the load plan ---------------------------------
//
// The pressure to fold them is real: both are project-level, both name artifacts, and
// a build that fed a load would be convenient to write in one file. It is refused,
// and the reason is the one this phase turns on. A load plan row is an
// EXECUTION-AUTHORITY DECISION -- *let this native artifact contribute executable
// semantic power to the host*, *let it participate as a weave under this identity* --
// and its whole legibility comes from being short enough to read in one sitting.
// Putting a source path, a package prefix list, a link list and a build tree beside
// each row would bury the authority decision inside a build procedure, and the two
// change for entirely different reasons and at entirely different rates.
//
// SO THE CROSS-REFERENCE IS ONE NAME AND NOTHING ELSE. A recipe says which ARTIFACT
// STEM it produces; a plan row IS an artifact stem. Nothing in this file carries a
// role, a mount mode or a load order, and nothing in the plan carries a compiler
// flag, a generator or a source list. Which artifacts a project can build and which
// artifacts it runs are two questions with two owners, joined by a string.
//
// ---- What it shares with the other three, and what it does not ----------------
//
// The same LOOM COMPAT CODEC (<zen/serialize.hpp>) for every reason persist.hpp
// gives: an already-linked dependency, the same gate the live bus uses, unknown-field
// rejection, kind validation, UTF-8 validation, a materialisation budget, and
// deterministic output that makes save -> load -> save byte-identical. ONE CODEC, and
// every door goes through it. It shares `persist::read_file` for the same reason
// `load_persist` does, with its own ceiling and its own word for what it is reading.
//
// ---- Why an optional kind is a LIST of at most one ----------------------------
//
// `load_persist.hpp`'s decision, taken here for its reason exactly: Zen's wire grammar
// has seven kinds and none is `optional`, so the honest spelling of "one of these two"
// is the kind that already means "zero or more", bounded to one by this format's own
// law and to EXACTLY one by the recipe law underneath it.
//
//     { "recipe": "oven", "artifact": "oven", "artifact_dir": "",
//       "cmake_target": [],
//       "single_source": [ { "source": "/home/me/oven.cpp",
//                            "packages": [ "/opt/zengine" ],
//                            "links": [ "zengine::timer", "loom::switchboard" ],
//                            "toolchain_from": "", "workspace": "" } ] }
//
// AN EMPTY STRING IS HOW AN OPTIONAL *STRING* IS SAID, and it is not the same choice.
// A path cannot be empty and mean anything, so `""` is unambiguous and needs no list
// -- and every field the shape declares is still present in every file, which is what
// the codec requires and what makes a diff of two recipe files readable.
//
// ---- What version 1 promises --------------------------------------------------
//
//   PROMISED   Workshop reads and writes build-recipe format version 1, and a second
//              save of a loaded catalog is byte-identical to the first.
//   REFUSED    any other `format_version`, with the number named; a `format` that is
//              not this one; a field the shape does not declare; a field of the wrong
//              kind; more than one mechanism on one recipe; anything the recipe law
//              refuses (an empty or traversing artifact stem, a name with a separator
//              in it, a link that is not a target name, a path with a control
//              character or a quote in it, a recipe declared twice); a file larger
//              than a catalog can be.
//   ACCEPTED   a recipe whose source file, build tree or package prefix is not on
//              this disk. That is authored intent and stays authored intent -- the
//              BUILD refuses it, by name, and nothing rewrites the entry.
//   NOT DONE   migration, a legacy reader, a version graph, an upgrade path, a dual
//              writer. There is no version 0 to migrate from.

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
///
/// Beside the binary for the load plan's reason, one step further: the shipped recipe
/// names this repository's own build tree, which is a fact about how this Workshop was
/// produced rather than about where a maker started it. A maker wanting a different
/// catalog passes `--recipes`; there is no registry, no picker and no search path.
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
///
/// FIVE LAYERS, IN ORDER, AND THE LAST ONE IS THE RECIPE'S OWN LAW: the envelope must
/// parse; its CLAIM must be this version; it must admit against this shape; it must
/// say it is this format at this version; and the catalog it describes must be a legal
/// catalog (`builder::check_recipes` -- the SAME function anything authoring recipes in
/// memory goes through, so a written catalog and a typed one cannot come to disagree).
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
///
/// Nothing in the production host calls this -- Workshop READS its recipes and never
/// writes them, because a host that rewrote its own authored intent is exactly what
/// this phase's non-goals forbid. It exists because a durable authored artifact whose
/// codec cannot be round-tripped is a codec nobody has checked.
inline Written save_file(const std::string& path, const std::vector<builder::Recipe>& recipes) {
    return persist::write_file(path, to_text(recipes));
}

/// FILL IN THE FACTS AN AUTHORED RECIPE CANNOT CARRY, and only those.
///
/// A recipe file is a thing a PROJECT carries: it is written by a maker who cannot know
/// where the binary reading it will be installed, and should not have to know which
/// directory their shell was in. Three answers are therefore the host's, and this is the
/// one place any of them is given -- because the moment two parties complete a recipe
/// independently, the recipe means two things.
///
///   `artifact_dir`  where the built file lands. Empty means beside the host's own
///                   artifacts: INSTALLATION truth, and `host_dir` is where this binary is.
///   `workspace`     where a generated project is written. Empty means the same place, for
///                   the same reason -- it is scratch this INSTALL owns.
///   `source`        the one file a `single_source` recipe compiles. A relative spelling
///                   means "relative to the PROJECT", which is where this Workshop was
///                   launched, and it is resolved here into a path that means one file.
///
/// THE THIRD IS THE ONE THAT IS NOT OPTIONAL, and it exists because leaving it undone was a
/// defect rather than a gap. Nothing wrote it down, so three parties each resolved the
/// authored spelling wherever they happened to stand: the editor and the runner's
/// exists-preflight against the PROCESS's working directory, and the generated CMake
/// project against the generated WORKSPACE, which CMake resolves `add_library` sources
/// against. A recipe naming `src/a.cpp` therefore named two different files, and only an
/// absolute spelling was safe. Resolved once, here, every later reader is reading one
/// answer -- which is what makes "the file you edit is the file the build reads" a property
/// of the structure rather than a coincidence three parties maintain.
///
/// THE FILE IS NEVER REWRITTEN. This completes the VALUE in memory; a catalog that recorded
/// this machine's paths would stop being a catalog a project can carry.
///
/// AN ABSENT PROJECT COMPLETES NOTHING. With no launch directory to stand on, a relative
/// spelling is left exactly as authored -- to be refused downstream, in words, by whoever
/// tries to spend it. Guessing a base is the one thing worse than the refusal.
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
