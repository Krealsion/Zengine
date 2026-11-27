// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROJECT_ACTS_HPP
#define ZENGINE_WORKSHOP_PROJECT_ACTS_HPP

// What the host's owners answer the project door (`pane_doors.hpp`) when it reads a recipe or
// spends an act on the recipes or the plan, and the one rule that spells an artifact stem as a
// file. Host-side values: the door hands them to no other weave, and answers in its own shapes.
// Workshop law: agents/workshop/project.md, agents/workshop/authoring.md

#include "builder/vocabulary.hpp" // a module's suffix, which the stem rule reads

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace zengine::workshop {

/// What the recipe catalog says about one recipe's source, answered by the host through the
/// read-only project role.
// WL-PROJ-02 -- agents/workshop/project.md
struct RecipeSource {
    bool known = false; ///< the id names an authored recipe of this project
    std::string kind;   ///< `single_source` or `cmake_target`, the file's own words
    std::string source; ///< the one authored source file; empty when the kind has none
};

/// WHAT A WEAVER'S CHOICE OF AUTHORED CATALOG ANSWERED.
// WL-PROJ-09 -- agents/workshop/project.md
struct RecipeSwap {
    bool accepted = false;   ///< the candidate became this session's catalog
    std::string refusal;     ///< the owner's own words; empty exactly when accepted
    std::string path;        ///< the authored catalog IN FORCE after this request
    std::size_t recipes = 0; ///< how many that catalog holds
};

/// A weaver's recipe row as typed. The host composes it, checks it by the recipe law, appends it
/// as authored and installs the file; the answer is the catalog in force (`RecipeSwap`).
// WL-AUTH-01 -- agents/workshop/authoring.md
struct RecipeDraft {
    std::string id;                    ///< what the weaver calls it
    std::string artifact;              ///< the stem it produces
    std::string source;                ///< single-source: the one .cpp, as the browser spelled it
    std::vector<std::string> packages; ///< single-source: CMAKE_PREFIX_PATH entries
    std::vector<std::string> links;    ///< single-source: exported target names
    std::string build_dir;             ///< cmake-target: the configured tree
    std::string target;                ///< cmake-target: the target in it
    std::string artifact_dir;          ///< cmake-target: where it lands, or empty
    std::string config;   ///< cmake-target: a multi-config generator's configuration, or empty
    bool tree = false;    ///< which of the two kinds this draft is
    std::string makes = {}; ///< single-source: native, boxed or both
};

/// What authoring the minimum plan row came to: appended and written, or refused in the plan's or
/// the executor's words -- and the recipe's product when the new row is the frontier and the
/// product exists, so `o` can finish with the button's own act.
// WL-AUTH-02, WL-AUTH-03 -- agents/workshop/authoring.md
struct PlanAppend {
    bool accepted = false;
    std::string refusal;   ///< empty exactly when accepted
    std::string path;      ///< the plan file written, when one was
    std::string detail;    ///< what the running project made of the row, in its words
    bool frontier = false; ///< the new row is what the project is now waiting on
    std::string product;   ///< the recipe's product on disk, when the row is the frontier and the
                           ///< product is there; empty otherwise
};

/// AN ARTIFACT STEM AS THIS PLATFORM SPELLS IT AS A FILE IN `directory`: the host's one rule, so
/// one plan is legal on every platform. A stem with WebAssembly's suffix (`builder::kModuleSuffix`)
/// is that file on every platform, which Loom's kernel opens by its first bytes and holds in a box;
/// any other is a shared library. A function of the platform alone, and one copy of the rule.
inline std::string so_in(std::string_view directory, std::string_view stem) {
    std::string path = std::string(directory) + "/" + std::string(stem);
    if (builder::names_module(stem)) {
        return path;
    }
    return path +
#if defined(_WIN32)
           ".dll";
#else
           ".so";
#endif
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_PROJECT_ACTS_HPP
