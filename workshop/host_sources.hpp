// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2026 Joshua DeMoss

#ifndef ZENGINE_WORKSHOP_HOST_SOURCES_HPP
#define ZENGINE_WORKSHOP_HOST_SOURCES_HPP

// The host may describe itself; it may not invent provider power: two zero-input Sources over
// facts the host already owns, mounted through a door that refuses anything that takes an
// argument (agents/operators.md). The bodies read their owners, which must outlive the catalog:
// `workshop.cpp` declares them first, and the rvalue overloads are deleted.

#include "recipes.hpp"

#include "operator/catalog.hpp"
#include "operator/operator.hpp"
#include "operator/source.hpp"

#include <zen/schema.hpp>
#include <zen/value.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace zengine::workshop {

/// WHO CONTRIBUTED THESE, said honestly. It is not a loaded artifact and does not
/// pretend to be one: `Catalog::mount` takes a null custody for a provider that is not
/// an image at all, which is exactly what an in-process host is. The name is the
/// ordinary dotted provider identity every other provider carries, so provenance
/// projections carry it with nothing taught about hosts.
inline constexpr const char* kHostProvider = "zengine.workshop.host";

inline constexpr const char* kProjectAnchorSource = "zengine.project.anchor";
inline constexpr const char* kRecipeCatalogSource = "zengine.recipes.catalog";

namespace detail {

/// A Source's input schema: no ports, and an IDENTITY all the same. Two Sources have
/// two different empty schemas because the name is hashed into the content id, which is
/// what makes an empty pack built for one of them refused at the other's door.
inline std::shared_ptr<const loom::Schema> no_inputs(const std::string& identity) {
    return loom::make_schema(identity + ".in", 1, std::vector<loom::Field>());
}

inline const std::shared_ptr<const loom::Schema>& project_anchor_schema() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::SchemaBuilder("zengine.ProjectAnchor", 1).field("anchor", loom::Kind::Text).build();
    return s;
}

inline const std::shared_ptr<const loom::Schema>& recipe_catalog_facts_schema() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::SchemaBuilder("zengine.RecipeCatalogFacts", 1)
            .field("source", loom::Kind::Text)
            .field("recipes", loom::Kind::Int)
            .build();
    return s;
}

inline const std::shared_ptr<const loom::Schema>& recipe_catalog_schema() {
    static const std::shared_ptr<const loom::Schema> s =
        loom::SchemaBuilder("zengine.RecipeCatalog", 1)
            .message("catalog", recipe_catalog_facts_schema())
            .build();
    return s;
}

} // namespace detail

/// The project-relative semantic anchor this host owns, read from the owner. Empty is the owner's
/// designed absence and is carried verbatim: a Source that invented a plausible path would be
/// worse than one that has none.
inline op::OperatorDef project_anchor_source(const std::string& project_dir) {
    const std::string identity = kProjectAnchorSource;
    return op::OperatorDef(
        identity, detail::no_inputs(identity), detail::project_anchor_schema(),
        [&project_dir](const loom::Value&) { return loom::Cell::text(project_dir); });
}
/// An owner with no name dies before the sample. Refused at compile time.
op::OperatorDef project_anchor_source(std::string&&) = delete;

/// Which authored catalog is in force and how many recipes it holds, in one read of one owner
/// (`CurrentRecipes` keeps the two together). The smallest standing description: the rows
/// themselves stay unexposed.
inline op::OperatorDef recipe_catalog_source(const CurrentRecipes& recipes) {
    const std::string identity = kRecipeCatalogSource;
    return op::OperatorDef(identity, detail::no_inputs(identity), detail::recipe_catalog_schema(),
                           [&recipes](const loom::Value&) {
                               loom::Value facts(detail::recipe_catalog_facts_schema());
                               facts.set("source", loom::Cell::text(recipes.source()));
                               facts.set("recipes", loom::Cell::integer(static_cast<std::int64_t>(
                                                        recipes.all().size())));
                               return loom::Cell::message(std::move(facts));
                           });
}
/// As above: a temporary owner is not an owner.
op::OperatorDef recipe_catalog_source(CurrentRecipes&&) = delete;

/// EVERY SOURCE THIS HOST EXPOSES, and the list is written out by hand on purpose.
inline std::vector<op::OperatorDef> host_sources(const std::string& project_dir,
                                                 const CurrentRecipes& recipes) {
    std::vector<op::OperatorDef> defs;
    defs.push_back(project_anchor_source(project_dir));
    defs.push_back(recipe_catalog_source(recipes));
    return defs;
}
std::vector<op::OperatorDef> host_sources(std::string&&, const CurrentRecipes&) = delete;
std::vector<op::OperatorDef> host_sources(const std::string&, CurrentRecipes&&) = delete;

/// The host's one door into its own catalog, and the only thing it carries: every definition is
/// judged `op::is_source` before any is installed, all or nothing. Underneath is `Catalog::mount`
/// with a null custody; there is no host-only registration path.
inline op::MountReport mount_host_sources(op::Catalog& into, std::vector<op::OperatorDef> defs) {
    for (const op::OperatorDef& def : defs) {
        if (!op::is_source(def)) {
            std::string ports;
            for (const loom::Field& f : def.inputs()->fields()) {
                ports += ports.empty() ? "" : ", ";
                ports += f.name;
            }
            return op::MountReport{false, "'" + def.identity() + "' takes " + ports +
                                              ", and a host may expose only sources over state it "
                                              "owns -- never parameterized power"};
        }
    }
    return into.mount(kHostProvider, std::move(defs));
}

} // namespace zengine::workshop

#endif // ZENGINE_WORKSHOP_HOST_SOURCES_HPP
