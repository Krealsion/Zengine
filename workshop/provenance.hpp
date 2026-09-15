// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_PROVENANCE_HPP
#define ZENGINE_WORKSHOP_PROVENANCE_HPP

// WHAT STANDS BEHIND A RUNNING OFFICE'S CODE -- the host's side of a pane's Edit Code.
//
// THREE EDGES, THREE OWNERS, ONE READING AT THE MOMENT OF THE ASK. Nothing here stores an
// answer, and no edge is inferred from another's spelling:
//
//     office   -> the weave holding it now            the bus (`Switchboard::role_holder`)
//     weave    -> the artifact that weave came from    the realization owner's resolved row
//     artifact -> every recipe that produces it        the recipe catalog in force
//
// ⚠ THE SECOND EDGE IS A WEAVEID AND NEVER A ROLE STRING. `ResolvedArtifact::role` is the
// AUTHORED role copied forward (arrangement_vocabulary.hpp says why there is no resolved one),
// so a plan row whose authored role matches the office proves nothing about who holds it: the
// row may have refused, a different weave may hold the office, or Workshop itself may. The
// holder's id and the row's minted id are the two owners' own facts, and only their equality
// is a join.
//
// ⚠ AND IT CHOOSES NOTHING. Several recipes may produce one artifact (the recipe law accepts
// it, `builder::check_recipes`); the answer lists them in catalog order and a consumer that
// cannot choose says so. A `cmake_target` recipe is listed with no source, because it names a
// configured tree and a target and no single file -- a source is never guessed for one.
//
// HOST-SIDE, for `staging.hpp`'s reason: it reads the realization owner's rows, and no
// presentation source may spell that owner. The desk reads the value through
// `HostContext::code_source`, wired over this in the host.
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
        }
        out.recipes.push_back(std::move(named));
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
