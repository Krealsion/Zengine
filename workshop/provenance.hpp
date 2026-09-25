// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROVENANCE_HPP
#define ZENGINE_WORKSHOP_PROVENANCE_HPP

// What stands behind a running office's code: the host's side of a pane's Edit Code. Three edges,
// read from three owners at the ask (office to weave, the bus; weave to artifact, the realization
// owner's row, joined by WeaveId and never by role string; artifact to recipes, the catalog), and
// nothing chosen. Host-side, because it reads the realization owner's rows.
// Workshop law: agents/workshop/code.md

#include "load_execute.hpp"
#include "recipes.hpp"
#include "weave.hpp"

#include "builder/recipe.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop::provenance {

/// WHAT STANDS BEHIND `office`, given the three owners' readings at this instant: the office's
/// holder (invalid when nobody holds it), the realization owner's resolved rows, and the
/// completed recipe catalog.
// WL-CODE-01 -- agents/workshop/code.md
inline HostContext::CodeSource code_source_of(const std::string& office, loom::WeaveId holder,
                                              const std::vector<load::ResolvedArtifact>& rows,
                                              const std::vector<builder::Recipe>& recipes) {
    HostContext::CodeSource out;
    out.office = office;
    if (!holder.valid()) {
        return out;
    }
    out.weave = static_cast<std::int64_t>(holder.value);
    const load::ResolvedArtifact* realized = nullptr;
    for (const load::ResolvedArtifact& row : rows) {
        if (row.weave_loaded && row.weave == holder) {
            realized = &row;
            break;
        }
    }
    if (realized == nullptr) {
        return out;
    }
    out.artifact = realized->stem;
    out.reload = load::reload_refusal(*realized);
    for (const builder::Recipe& recipe : recipes) {
        if (recipe.artifact != out.artifact) {
            continue;
        }
        HostContext::CodeSource::Recipe named;
        named.id = recipe.id;
        named.kind = recipe.single_source.has_value() ? "single_source" : "cmake_target";
        if (recipe.single_source.has_value()) {
            named.source = recipe.single_source->source;
        } else if (recipe.cmake_target.has_value()) {
            named.source = recipe.cmake_target->entry;
        }
        out.recipes.push_back(std::move(named));
    }
    return out;
}

/// WHICH FILE ONE RECIPE'S CODE STARTS IN -- the answer `HostContext::recipe_source` gives the
/// read-only project door, read from the catalog in force at the ask: a single source, or the
/// editing entry a `cmake_target` recipe names, completed as the build reads them; empty for a
/// recipe that names neither. One function, so the Builder's `e` and Edit Code read a recipe the
/// same way, and a suite can ask the rule the host wires rather than a copy of it.
// WL-CODE-05 -- agents/workshop/code.md
inline HostContext::RecipeSource recipe_source_of(const std::vector<builder::Recipe>& recipes,
                                                  const std::string& id) {
    HostContext::RecipeSource out;
    const builder::Recipe* found = builder::recipe_named(recipes, id);
    if (found == nullptr) {
        return out;
    }
    out.known = true;
    out.kind = found->single_source.has_value() ? "single_source" : "cmake_target";
    if (found->single_source.has_value()) {
        out.source = found->single_source->source;
    } else if (found->cmake_target.has_value()) {
        out.source = found->cmake_target->entry;
    }
    return out;
}

/// THE SAME READING, TAKEN FROM THE OWNERS THEMSELVES at this instant: who holds the office on
/// this bus, the rows this realization owner has resolved, and the catalog in force. What the
/// host wires into `HostContext::code_source`; it reads, and it drives nothing.
// WL-CODE-01 -- agents/workshop/code.md
inline HostContext::CodeSource code_source_of(const std::string& office,
                                              const loom::Switchboard& bus,
                                              const load::PlanExecutor& realization,
                                              const CurrentRecipes& recipes) {
    return code_source_of(office, bus.role_holder(office), realization.resolved(), recipes.all());
}

} // namespace zengine::workshop::provenance

#endif // ZENGINE_WORKSHOP_PROVENANCE_HPP
