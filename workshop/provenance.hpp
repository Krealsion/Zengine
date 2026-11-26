// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROVENANCE_HPP
#define ZENGINE_WORKSHOP_PROVENANCE_HPP

// The host's readings of the owners it holds: the catalog in force as the project door claims it,
// which file one recipe names, and what stands behind a role's code read from the owners
// themselves -- the same composition Workshop makes of their claims (`code_source.hpp`), for a
// host or a suite that holds the owners. Host-side, because it reads the realization owner's rows.
// Workshop law: agents/workshop/code.md

#include "code_source.hpp"
#include "realization/owner.hpp"
#include "recipes.hpp"
#include "weave.hpp"

#include "builder/recipe.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::provenance {

/// THE CATALOG IN FORCE AS ITS OWNER CLAIMS IT (`RecipesInForce`): each recipe's name, kind,
/// the file its code starts in and every product, read from the completed catalog and the file it
/// came from. One function, so the claim and a host reading the catalog cannot disagree.
// WL-CODE-01 -- agents/workshop/code.md
inline RecipesInForce recipes_in_force_of(const std::vector<builder::Recipe>& recipes,
                                          const std::string& path) {
    RecipesInForce out;
    out.path = path;
    for (const builder::Recipe& recipe : recipes) {
        RecipeInForce one;
        one.id = recipe.id;
        one.kind = recipe.single_source.has_value() ? "single_source" : "cmake_target";
        if (recipe.single_source.has_value()) {
            one.source = recipe.single_source->source;
        } else if (recipe.cmake_target.has_value()) {
            one.source = recipe.cmake_target->entry;
        }
        for (const builder::Product& product : builder::products_of(recipe)) {
            one.products.push_back(product.artifact);
        }
        out.recipes.push_back(std::move(one));
    }
    return out;
}

/// The same, of the catalog this host holds.
inline RecipesInForce recipes_in_force_of(const CurrentRecipes& recipes) {
    return recipes_in_force_of(recipes.all(), recipes.source());
}

/// WHAT STANDS BEHIND `role`, given the three owners' readings at this instant: the role's
/// holder (invalid when nobody holds it), the realization owner's resolved rows and the completed
/// recipe catalog -- composed as Workshop composes their claims.
// WL-CODE-01 -- agents/workshop/code.md
inline CodeSource code_source_of(const std::string& role, loom::WeaveId holder,
                                 const std::vector<realization::ResolvedArtifact>& rows,
                                 const std::vector<builder::Recipe>& recipes) {
    const realization::RealizedRows realized = realization::realized_rows_of(rows);
    const RecipesInForce catalog = recipes_in_force_of(recipes, std::string());
    return workshop::code_source_of(role, static_cast<std::int64_t>(holder.value), &realized,
                                    &catalog);
}

/// WHICH FILE ONE RECIPE'S CODE STARTS IN -- the answer `HostContext::recipe_source` gives the
/// read-only project door, read from the catalog in force at the ask: a single source, or the
/// editing entry a `cmake_target` recipe names, completed as the build reads them; empty for a
/// recipe that names neither. Read through the claim's own rule (`recipes_in_force_of`), so the
/// Builder's `e` and Edit Code read a recipe the same way, and a suite can ask the rule the host
/// wires rather than a copy of it.
// WL-CODE-05 -- agents/workshop/code.md
inline HostContext::RecipeSource recipe_source_of(const std::vector<builder::Recipe>& recipes,
                                                  const std::string& id) {
    HostContext::RecipeSource out;
    const builder::Recipe* found = builder::recipe_named(recipes, id);
    if (found == nullptr) {
        return out;
    }
    const RecipesInForce one = recipes_in_force_of({*found}, std::string());
    out.known = true;
    out.kind = one.recipes.front().kind;
    out.source = one.recipes.front().source;
    return out;
}

/// THE SAME READING, TAKEN FROM THE OWNERS THEMSELVES at this instant: who holds the role on
/// this bus, the rows this realization owner has resolved, and the catalog in force. It reads, and
/// it drives nothing.
// WL-CODE-01 -- agents/workshop/code.md
inline CodeSource code_source_of(const std::string& role, const loom::Switchboard& bus,
                                 const realization::Owner& realization,
                                 const CurrentRecipes& recipes) {
    return code_source_of(role, bus.role_holder(role), realization.resolved(), recipes.all());
}

} // namespace zengine::workshop::provenance

#endif // ZENGINE_WORKSHOP_PROVENANCE_HPP
