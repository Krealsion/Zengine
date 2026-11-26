// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_CODE_SOURCE_HPP
#define ZENGINE_WORKSHOP_CODE_SOURCE_HPP

// What stands behind a running role's code, composed by Workshop inside the delivery that needs
// it from three owners' latest claims: Loom's participants claim (role to weave), the realization
// owner's realized rows (weave to artifact, joined by WeaveId and never by role string) and the
// project door's catalog in force (artifact to recipes). Nothing is asked and nothing waits; an
// owner that claims nothing is an absence the answer names.
// Workshop law: agents/workshop/code.md

#include "realization/vocabulary.hpp"
#include "workshop/project_seam_vocabulary.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// What stands behind one role's running code, read at the ask from the three owners of its
/// edges. Empty fields are absences a sentence names; choosing a recipe and opening anything are
/// not this answer's.
// WL-CODE-01 -- agents/workshop/code.md
struct CodeSource {
    /// One authored recipe producing the artifact. `source` is a single-source recipe's file as
    /// the build reads it -- an editing entry, not its includes -- or a CMake target's entry.
    struct Recipe {
        std::string id;
        std::string kind;   ///< `single_source` or `cmake_target`, the recipe file's words
        std::string source; ///< empty when the recipe names no file to start from
    };
    std::string role;            ///< the role asked about
    std::int64_t weave = 0;      ///< its holder at the ask; 0 when nobody holds it
    bool rows_claimed = false;   ///< the realization owner claims what it realized
    std::string artifact;        ///< the stem that weave was realized from; empty if none
    std::string reload;          ///< why a rebuilt image cannot reload in place; empty if it can
    bool catalog_claimed = false; ///< the project door claims the catalog in force
    std::vector<Recipe> recipes; ///< every authored recipe producing `artifact`, catalog order
};

/// WHAT STANDS BEHIND `role`, given the three readings at this instant: the role's holder (0 when
/// nobody holds it), the realization owner's realized rows and the catalog in force, each null
/// when its owner claims nothing the reader may read.
// WL-CODE-01 -- agents/workshop/code.md
inline CodeSource code_source_of(const std::string& role, std::int64_t holder,
                                 const realization::RealizedRows* realized,
                                 const RecipesInForce* catalog) {
    CodeSource out;
    out.role = role;
    out.weave = holder;
    out.rows_claimed = realized != nullptr;
    out.catalog_claimed = catalog != nullptr;
    if (holder == 0 || realized == nullptr) {
        return out;
    }
    const realization::RealizedRow* row = nullptr;
    for (const realization::RealizedRow& one : realized->rows) {
        if (one.weave == holder) {
            row = &one;
            break;
        }
    }
    if (row == nullptr) {
        return out;
    }
    out.artifact = row->stem;
    out.reload = row->reload;
    if (catalog == nullptr) {
        return out;
    }
    for (const RecipeInForce& recipe : catalog->recipes) {
        for (const std::string& product : recipe.products) {
            if (product == out.artifact) {
                out.recipes.push_back(CodeSource::Recipe{recipe.id, recipe.kind, recipe.source});
                break;
            }
        }
    }
    return out;
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_CODE_SOURCE_HPP
