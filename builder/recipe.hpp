// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_BUILDER_RECIPE_HPP
#define ZENGINE_BUILDER_RECIPE_HPP

// What a build recipe is: authored knowledge of how one artifact can be produced -- an
// identity, the artifact stem it makes, and one build mechanism with its inputs. It is build
// procedure, never runtime participation (that is the load plan's), and the two meet only at
// the artifact stem. The program is always the host's CMake; there is no `command` kind and
// must not be, or every recipe file would be an arbitrary-execution document.
// Builder law: agents/realization.md

// A refusal is a `std::string`, empty meaning accepted: builder/ sits below workshop/, so it
// does not borrow `workshop::Written` (workshop/recipe_persist.hpp wraps these at the file).

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace zengine::builder {

// ---- What a recipe may hold -----------------------------------------------------

/// How many recipes one catalog may hold: a bound on a forged file, read before anything is
/// built -- a hostile one does not choose the cost of refusing it.
inline constexpr std::size_t kMaxRecipes = 32;

/// How long a recipe identity may be. A routing-name bound for a routing-shaped name.
inline constexpr std::size_t kMaxRecipeIdLen = 64;

/// How long any path a recipe names may be: one authored string, not the operating system's
/// own limit.
inline constexpr std::size_t kMaxRecipePathLen = 1024;

/// How many CMake packages a single-source recipe may be pointed at, and how many
/// exported targets it may link. Two small numbers because both lists are hand-written
/// by a maker describing ONE source file's public dependencies; a source file needing
/// more than this is a project, and a project has a CMakeLists.
inline constexpr std::size_t kMaxRecipePackages = 8;
inline constexpr std::size_t kMaxRecipeLinks = 16;

/// How long one link target name may be (`zengine::operator-consumer` is twenty-eight).
inline constexpr std::size_t kMaxLinkTargetLen = 64;

// ---- The command a recipe becomes ------------------------------------------------

/// One process, as the runner will start it: a program and an argument vector, never a command
/// line, so no misplaced quote can become an extra command. Every command this package produces
/// has `program` set to the host's own CMake. Derived from a recipe, never authored.
struct BuildCommand {
    std::string program;           ///< an executable, chosen by this package and never authored
    std::vector<std::string> args; ///< its arguments, already separated
    std::string dir;               ///< the working directory to run it in ("" = inherit)

    /// The command as one readable line, for a maker who wants to know what a button
    /// actually did. Deliberately not a re-runnable command line: it is a description,
    /// and nothing parses it back.
    std::string as_line() const {
        std::string line = program;
        for (const std::string& a : args) {
            line += ' ';
            line += a;
        }
        return line;
    }
};

// ---- The two authored kinds -------------------------------------------------------

/// An artifact a CMake project already owns: a configured build tree and a target, built with
/// `cmake --build <tree> --target <target>`. A configured tree, never a source tree: configuring
/// somebody else's project would be deciding its policy. `config` is for a multi-config
/// generator (single-config ones ignore it). `entry` is where a reader of the code begins, an
/// editing entry and nothing more: nothing builds from it, and none is guessed.
struct CMakeTargetRecipe {
    std::string build_dir; ///< a CONFIGURED CMake build tree
    std::string target;    ///< the target in it that produces this recipe's artifact
    std::string config;    ///< a multi-config generator's configuration, or empty
    std::string entry;     ///< the source file editing starts from, or empty

    friend bool operator==(const CMakeTargetRecipe&, const CMakeTargetRecipe&) = default;
};

/// One C++ source file, around which Zengine generates a tiny CMake project
/// (builder/generate.hpp) and CMake does the rest. `packages` is `CMAKE_PREFIX_PATH`, the only
/// place the project finds Zengine, so it is a true external consumer. `links` are exported
/// target names, never a link line. `toolchain_from` is a configured tree whose toolchain is
/// borrowed (empty: CMake chooses). `workspace` is where the project is generated, durable so its
/// diagnostics survive (empty: the host's choice).
struct SingleSourceRecipe {
    std::string source;                    ///< the one .cpp a maker wrote
    std::vector<std::string> packages;     ///< CMAKE_PREFIX_PATH entries
    std::vector<std::string> links;        ///< exported CMake target names to link
    std::string toolchain_from;            ///< a configured build tree to borrow a toolchain from
    std::string workspace;                 ///< where to generate the project ("" = host's choice)

    friend bool operator==(const SingleSourceRecipe&, const SingleSourceRecipe&) = default;
};

/// One authored build recipe, with exactly one mechanism: neither describes nothing and both
/// describe two builds under one name, so both are refused rather than resolved by precedence.
/// `artifact` is a stem, spelled to a file by the host's one rule as a load plan's is, so a
/// recipe and a plan row match by exact string. `artifact_dir` is a directory, empty meaning the
/// host's artifact directory; a file's name and suffix are never a maker's to spell.
struct Recipe {
    std::string id;           ///< what a maker and the tool call this recipe
    std::string artifact;     ///< the artifact STEM this recipe is expected to produce
    std::string artifact_dir; ///< where that artifact lands ("" = the host's artifact directory)
    std::optional<CMakeTargetRecipe> cmake_target;
    std::optional<SingleSourceRecipe> single_source;

    friend bool operator==(const Recipe&, const Recipe&) = default;
};

/// What the Builder tool is told about a recipe, deliberately less: an identity, its artifact
/// and the one file that artifact means -- no build tree, source, prefix, link list or command,
/// since the tool is only asked which recipe and whether the artifact appeared. `path` is
/// resolved by the host from `artifact_dir` and its own rule for spelling a stem.
struct RecipeView {
    std::string id;
    std::string artifact;
    std::string path; ///< the exact file this recipe is expected to produce

    friend bool operator==(const RecipeView&, const RecipeView&) = default;
};

// ---- The recipe's own law ----------------------------------------------------------

/// What this application accepts as a recipe identity: a short printable name, never a path.
inline std::string check_recipe_id(const std::string& id) {
    if (id.empty()) {
        return "a recipe needs a name";
    }
    if (id.size() > kMaxRecipeIdLen) {
        return "a recipe name is at most " + std::to_string(kMaxRecipeIdLen) + " bytes";
    }
    for (const char c : id) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return "recipe name `" + id + "` cannot contain spaces or control characters";
        }
        if (c == '/' || c == '\\') {
            return "recipe name `" + id + "` cannot contain a path separator: it is a name, not "
                                          "a location";
        }
    }
    return std::string();
}

/// What this application accepts as an artifact stem: the load plan's rules, restated because
/// the two files sit on either side of each other in this tree, and checked in both because a
/// stem is a file this host will execute.
inline std::string check_recipe_artifact(const std::string& stem) {
    if (stem.empty()) {
        return "a recipe must say which artifact it produces";
    }
    if (stem.size() > kMaxRecipeIdLen) {
        return "an artifact stem is at most " + std::to_string(kMaxRecipeIdLen) + " bytes";
    }
    for (const char c : stem) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte <= ' ' || byte == 0x7Fu) {
            return "artifact stem `" + stem + "` cannot contain spaces or control characters";
        }
        if (c == '/' || c == '\\') {
            return "artifact stem `" + stem +
                   "` cannot contain a path separator: a stem names a file, and where that "
                   "file lands is the recipe's `artifact_dir`";
        }
    }
    if (stem.find("..") != std::string::npos) {
        return "artifact stem `" + stem + "` cannot contain `..`";
    }
    return std::string();
}

/// What this application accepts as a path a recipe names: spaces are legal (a real checkout
/// lives under `My Documents`), control characters and quotes are not -- a newline or NUL ends
/// a line in a generated script and a quote ends a string in one, so they are refused.
inline std::string check_recipe_path(const std::string& what, const std::string& path) {
    if (path.empty()) {
        return what + " cannot be empty";
    }
    if (path.size() > kMaxRecipePathLen) {
        return what + " is at most " + std::to_string(kMaxRecipePathLen) + " bytes";
    }
    for (const char c : path) {
        const unsigned char byte = static_cast<unsigned char>(c);
        if (byte < ' ' || byte == 0x7Fu) {
            return what + " cannot contain control characters";
        }
        if (c == '"') {
            return what + " cannot contain a double quote";
        }
    }
    return std::string();
}

/// What this application accepts as an exported CMake target to link: a name, never a flag, a
/// path or a way out of the argument it is written in.
inline std::string check_link_target(const std::string& target) {
    if (target.empty()) {
        return "a link target cannot be empty";
    }
    if (target.size() > kMaxLinkTargetLen) {
        return "a link target is at most " + std::to_string(kMaxLinkTargetLen) + " bytes";
    }
    // A leading `-` is a flag: `-` is legal inside a name (`zengine::operator-consumer`), so
    // `-lpthread` would pass every other rule here.
    if (target.front() == '-') {
        return "link target `" + target +
               "` starts with `-`: a target is linked by NAME, and a linker flag is not one";
    }
    for (const char c : target) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == ':' || c == '-' || c == '_' || c == '.' ||
                        c == '+';
        if (!ok) {
            return "link target `" + target +
                   "` is not a CMake target name: a target is linked by NAME, and a flag, a "
                   "path or a library file is not one";
        }
    }
    return std::string();
}

/// EVERY LAW ONE RECIPE MEETS, minus the one that is about the whole catalog.
inline std::string check_recipe(const Recipe& r) {
    const std::string id = check_recipe_id(r.id);
    if (!id.empty()) {
        return id;
    }
    const std::string stem = check_recipe_artifact(r.artifact);
    if (!stem.empty()) {
        return "recipe `" + r.id + "`: " + stem;
    }
    if (!r.artifact_dir.empty()) {
        const std::string dir = check_recipe_path("an artifact directory", r.artifact_dir);
        if (!dir.empty()) {
            return "recipe `" + r.id + "`: " + dir;
        }
    }
    const int kinds = (r.cmake_target.has_value() ? 1 : 0) + (r.single_source.has_value() ? 1 : 0);
    if (kinds == 0) {
        return "recipe `" + r.id + "` names no build mechanism";
    }
    if (kinds > 1) {
        return "recipe `" + r.id +
               "` names two build mechanisms: one recipe is one procedure, and which of two "
               "would run is not a question a maker should have to answer from a precedence "
               "rule";
    }
    if (r.cmake_target.has_value()) {
        const std::string dir = check_recipe_path("a CMake build tree", r.cmake_target->build_dir);
        if (!dir.empty()) {
            return "recipe `" + r.id + "`: " + dir;
        }
        if (r.cmake_target->target.empty()) {
            return "recipe `" + r.id + "` names a CMake build tree and no target in it";
        }
        const std::string target = check_link_target(r.cmake_target->target);
        if (!target.empty()) {
            return "recipe `" + r.id + "`: `" + r.cmake_target->target +
                   "` is not a CMake target name";
        }
        if (!r.cmake_target->config.empty()) {
            const std::string cfg = check_link_target(r.cmake_target->config);
            if (!cfg.empty()) {
                return "recipe `" + r.id + "`: `" + r.cmake_target->config +
                       "` is not a CMake configuration name";
            }
        }
        if (!r.cmake_target->entry.empty()) {
            const std::string entry = check_recipe_path("an editing entry", r.cmake_target->entry);
            if (!entry.empty()) {
                return "recipe `" + r.id + "`: " + entry;
            }
        }
        return std::string();
    }
    const SingleSourceRecipe& one = *r.single_source;
    const std::string source = check_recipe_path("a source file", one.source);
    if (!source.empty()) {
        return "recipe `" + r.id + "`: " + source;
    }
    if (one.packages.size() > kMaxRecipePackages) {
        return "recipe `" + r.id + "` names more than " + std::to_string(kMaxRecipePackages) +
               " package prefixes";
    }
    for (const std::string& p : one.packages) {
        const std::string bad = check_recipe_path("a package prefix", p);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    if (one.links.empty()) {
        return "recipe `" + r.id +
               "` links nothing: a loadable Zengine artifact needs at least the Loom targets "
               "its own headers name, and a generated project that linked nothing would fail "
               "later and less clearly";
    }
    if (one.links.size() > kMaxRecipeLinks) {
        return "recipe `" + r.id + "` links more than " + std::to_string(kMaxRecipeLinks) +
               " targets";
    }
    for (const std::string& l : one.links) {
        const std::string bad = check_link_target(l);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    if (!one.toolchain_from.empty()) {
        const std::string bad = check_recipe_path("a toolchain build tree", one.toolchain_from);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    if (!one.workspace.empty()) {
        const std::string bad = check_recipe_path("a build workspace", one.workspace);
        if (!bad.empty()) {
            return "recipe `" + r.id + "`: " + bad;
        }
    }
    return std::string();
}

/// Every law a whole catalog meets. Duplicates are refused by identity, not by artifact: two
/// recipes of one name cannot be told apart, while two procedures for one artifact are an
/// ordinary want. An empty catalog is legal: a project with nothing to build is a project.
inline std::string check_recipes(const std::vector<Recipe>& recipes) {
    if (recipes.size() > kMaxRecipes) {
        return "a build recipe catalog names at most " + std::to_string(kMaxRecipes) + " recipes";
    }
    for (std::size_t i = 0; i < recipes.size(); ++i) {
        const std::string row = check_recipe(recipes[i]);
        if (!row.empty()) {
            return row;
        }
        for (std::size_t k = 0; k < i; ++k) {
            if (recipes[k].id == recipes[i].id) {
                return "recipe `" + recipes[i].id +
                       "` is declared twice: a recipe name is how a maker asks for one";
            }
        }
    }
    return std::string();
}

// ---- Finding one ------------------------------------------------------------------

/// The recipe with this name, or null. Written once because three readers need it.
inline const Recipe* recipe_named(const std::vector<Recipe>& recipes, const std::string& id) {
    for (const Recipe& r : recipes) {
        if (r.id == id) {
            return &r;
        }
    }
    return nullptr;
}

/// The tool's view of the recipe with this name, or null.
inline const RecipeView* view_named(const std::vector<RecipeView>& views, const std::string& id) {
    for (const RecipeView& v : views) {
        if (v.id == id) {
            return &v;
        }
    }
    return nullptr;
}

} // namespace zengine::builder

#endif // ZENGINE_BUILDER_RECIPE_HPP
